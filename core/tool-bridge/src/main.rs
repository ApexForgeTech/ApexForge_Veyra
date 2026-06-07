use std::collections::HashMap;
use std::env;
use std::io::{self, BufRead, Write};
use std::process::{Child, Command, Stdio};
use std::thread;
use std::time::{Duration, Instant};

struct Invocation {
    id: String,
    child: Child,
    started_at: Instant,
    max_seconds: u64,
    output_lines: Vec<(String, String)>,
    exit_code: Option<i32>,
    failed: bool,
    failure_reason: String,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 || args[1] != "--stdio" {
        eprintln!("Tool bridge requires --stdio mode.");
        std::process::exit(1);
    }

    if let Err(e) = run_stdio() {
        eprintln!("{e}");
        std::process::exit(1);
    }
}

fn run_stdio() -> io::Result<()> {
    let stdin = io::stdin();
    let mut stdout = io::stdout();
    let mut invocations: HashMap<String, Invocation> = HashMap::new();

    for line_result in stdin.lock().lines() {
        let line = line_result?;
        if line.is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.splitn(20, '\t').collect();
        match fields.first().copied().unwrap_or_default() {
            "INVOKE" => {
                handle_invoke(&fields, &mut invocations, &mut stdout)?;
            }
            "STATUS" => {
                handle_status(&fields, &mut invocations, &mut stdout)?;
            }
            "CANCEL" => {
                handle_cancel(&fields, &mut invocations, &mut stdout)?;
            }
            "SHUTDOWN" => {
                for inv in invocations.values_mut() {
                    let _ = inv.child.kill();
                    let _ = inv.child.wait();
                }
                writeln!(stdout, "OK\tbye")?;
                stdout.flush()?;
                break;
            }
            _ => {
                writeln!(stdout, "ERROR\tunknown command")?;
                stdout.flush()?;
            }
        }

        reap_completed(&mut invocations);
    }

    Ok(())
}

// INVOKE\t<invocation_id>\t<binary>\t<max_seconds>\t<capture_output>\t[arg0]\t[arg1]...
fn handle_invoke(
    fields: &[&str],
    invocations: &mut HashMap<String, Invocation>,
    stdout: &mut impl Write,
) -> io::Result<()> {
    if fields.len() < 5 {
        writeln!(stdout, "ERROR\tINVOKE requires at least 4 fields")?;
        stdout.flush()?;
        return Ok(());
    }

    let invocation_id = fields[1].to_string();
    let tool_binary = fields[2].to_string();
    let max_seconds: u64 = fields[3].parse().unwrap_or(60);
    let capture_output: bool = fields[4] == "1" || fields[4] == "true";
    let tool_args: Vec<String> = fields[5..].iter().map(|s| s.to_string()).collect();

    if invocations.contains_key(&invocation_id) {
        writeln!(stdout, "ERROR\tinvocation id already active: {invocation_id}")?;
        stdout.flush()?;
        return Ok(());
    }

    let child_result = Command::new(&tool_binary)
        .args(&tool_args)
        .stdin(Stdio::null())
        .stdout(if capture_output { Stdio::piped() } else { Stdio::null() })
        .stderr(if capture_output { Stdio::piped() } else { Stdio::null() })
        .spawn();

    match child_result {
        Err(e) => {
            writeln!(stdout, "FAILED\t{invocation_id}\tspawn-error\t{e}")?;
            writeln!(stdout, "OK\t0")?;
            stdout.flush()?;
        }
        Ok(mut child) => {
            let pid = child.id();
            writeln!(stdout, "STARTED\t{invocation_id}\t{pid}")?;
            stdout.flush()?;

            // Read stdout and stderr concurrently via threads to prevent deadlock.
            // If we read them sequentially, a child that writes enough to fill the
            // unread pipe's buffer will block before closing the other pipe, causing
            // the reader thread on the first pipe to wait forever.
            let output_lines: Vec<(String, String)> = if capture_output {
                collect_output_concurrent(&mut child)
            } else {
                Vec::new()
            };

            let exit_code = poll_for_exit(&mut child, max_seconds);

            for (stream, line) in &output_lines {
                let escaped = escape_output(line);
                writeln!(stdout, "OUTPUT\t{invocation_id}\t{stream}\t{escaped}")?;
            }

            let inv_id = invocation_id.clone();

            if let Some(code) = exit_code {
                writeln!(stdout, "COMPLETED\t{inv_id}\t{code}")?;
            } else {
                let _ = child.kill();
                let _ = child.wait();
                writeln!(stdout, "FAILED\t{inv_id}\ttimeout\ttool exceeded {max_seconds}s limit")?;
            }

            writeln!(stdout, "OK\t1")?;
            stdout.flush()?;

            let invocation = Invocation {
                id: invocation_id.clone(),
                child,
                started_at: Instant::now(),
                max_seconds,
                output_lines,
                exit_code,
                failed: exit_code.is_none(),
                failure_reason: if exit_code.is_none() {
                    format!("timeout after {max_seconds}s")
                } else {
                    String::new()
                },
            };
            invocations.insert(invocation_id, invocation);
        }
    }

    Ok(())
}

// STATUS\t<invocation_id>
fn handle_status(
    fields: &[&str],
    invocations: &mut HashMap<String, Invocation>,
    stdout: &mut impl Write,
) -> io::Result<()> {
    let invocation_id = fields.get(1).copied().unwrap_or_default();

    match invocations.get(invocation_id) {
        None => {
            writeln!(stdout, "ERROR\tunknown invocation id: {invocation_id}")?;
        }
        Some(inv) => {
            if let Some(code) = inv.exit_code {
                writeln!(stdout, "COMPLETED\t{invocation_id}\t{code}")?;
            } else if inv.failed {
                writeln!(stdout, "FAILED\t{invocation_id}\t{}", inv.failure_reason)?;
            } else {
                let elapsed = inv.started_at.elapsed().as_secs();
                writeln!(stdout, "RUNNING\t{invocation_id}\t{elapsed}")?;
            }
            writeln!(stdout, "OK\t1")?;
        }
    }

    stdout.flush()?;
    Ok(())
}

// CANCEL\t<invocation_id>
fn handle_cancel(
    fields: &[&str],
    invocations: &mut HashMap<String, Invocation>,
    stdout: &mut impl Write,
) -> io::Result<()> {
    let invocation_id = fields.get(1).copied().unwrap_or_default();

    match invocations.get_mut(invocation_id) {
        None => {
            writeln!(stdout, "ERROR\tunknown invocation id: {invocation_id}")?;
        }
        Some(inv) => {
            let _ = inv.child.kill();
            let _ = inv.child.wait();
            inv.failed = true;
            inv.failure_reason = "cancelled".to_string();
            writeln!(stdout, "OK\t1")?;
        }
    }

    stdout.flush()?;
    Ok(())
}

// Reads stdout and stderr concurrently using two threads to prevent pipe-buffer
// deadlock. Each thread reads its pipe to EOF and sends lines through a channel.
fn collect_output_concurrent(child: &mut Child) -> Vec<(String, String)> {
    use std::sync::mpsc;

    let (tx, rx) = mpsc::channel::<(String, String)>();

    let stdout_handle = child.stdout.take().map(|pipe| {
        let tx_clone = tx.clone();
        thread::spawn(move || {
            use std::io::BufReader;
            for line in BufReader::new(pipe).lines().filter_map(|l| l.ok()) {
                if tx_clone.send(("stdout".to_string(), line)).is_err() {
                    break;
                }
            }
        })
    });

    let stderr_handle = child.stderr.take().map(|pipe| {
        let tx_clone = tx.clone();
        thread::spawn(move || {
            use std::io::BufReader;
            for line in BufReader::new(pipe).lines().filter_map(|l| l.ok()) {
                if tx_clone.send(("stderr".to_string(), line)).is_err() {
                    break;
                }
            }
        })
    });

    // Drop the original sender so the channel closes when both threads finish.
    drop(tx);

    let mut lines: Vec<(String, String)> = rx.into_iter().collect();

    if let Some(h) = stdout_handle { let _ = h.join(); }
    if let Some(h) = stderr_handle { let _ = h.join(); }

    // Sort stdout lines before stderr lines for consistent ordering when both
    // are present, since channel order from two threads is non-deterministic.
    lines.sort_by(|a, b| a.0.cmp(&b.0));

    lines
}

fn poll_for_exit(child: &mut Child, max_seconds: u64) -> Option<i32> {
    let deadline = Instant::now() + Duration::from_secs(max_seconds);
    loop {
        match child.try_wait() {
            Ok(Some(status)) => return Some(status.code().unwrap_or(-1)),
            Ok(None) => {
                if Instant::now() >= deadline {
                    return None;
                }
                thread::sleep(Duration::from_millis(50));
            }
            Err(_) => return Some(-1),
        }
    }
}

fn reap_completed(invocations: &mut HashMap<String, Invocation>) {
    let completed: Vec<String> = invocations
        .iter_mut()
        .filter_map(|(id, inv)| {
            if inv.exit_code.is_some() || inv.failed {
                Some(id.clone())
            } else {
                match inv.child.try_wait() {
                    Ok(Some(status)) => {
                        inv.exit_code = Some(status.code().unwrap_or(-1));
                        Some(id.clone())
                    }
                    _ => None,
                }
            }
        })
        .collect();

    for id in completed {
        invocations.remove(&id);
    }
}

fn escape_output(s: &str) -> String {
    s.replace('\\', "\\\\").replace('\t', "\\t").replace('\n', "\\n").replace('\r', "\\r")
}
