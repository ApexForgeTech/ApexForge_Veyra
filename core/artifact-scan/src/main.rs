use sha1::{Digest as Sha1Digest, Sha1};
use sha2::Sha256;
use std::env;
use std::fs;
use std::io::{self, BufRead, Write};
use std::path::Path;

#[derive(Clone, Debug)]
struct ScanRequest {
    artifact_id: String,
    quarantine_path: String,
    source_url: String,
    suggested_filename: String,
}

#[derive(Clone, Debug)]
struct ScanReport {
    artifact_id: String,
    quarantine_path: String,
    source_url: String,
    suggested_filename: String,
    detected_name: String,
    size_bytes: u64,
    mime_guess: String,
    sha256: String,
    sha1: String,
    risk_score: i32,
    risk_level: String,
    heuristics: Vec<String>,
    summary: String,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 || args[1] != "--stdio" {
        eprintln!("Artifact scan service requires --stdio mode.");
        std::process::exit(1);
    }

    if let Err(error) = run_stdio() {
        eprintln!("{error}");
        std::process::exit(1);
    }
}

fn run_stdio() -> io::Result<()> {
    let stdin = io::stdin();
    let mut stdout = io::stdout();

    for line_result in stdin.lock().lines() {
        let line = line_result?;
        if line.is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.split('\t').collect();
        match fields.first().copied().unwrap_or_default() {
            "SCAN" => {
                let Some(request) = parse_request(&fields) else {
                    writeln!(stdout, "ERROR\tinvalid scan request")?;
                    stdout.flush()?;
                    continue;
                };
                match scan_file(&request) {
                    Ok(report) => {
                        writeln!(stdout, "{}", format_report(&report))?;
                        writeln!(stdout, "OK\t1")?;
                    }
                    Err(error) => {
                        writeln!(stdout, "ERROR\t{error}")?;
                    }
                }
                stdout.flush()?;
            }
            "SHUTDOWN" => {
                writeln!(stdout, "OK\tbye")?;
                stdout.flush()?;
                break;
            }
            _ => {
                writeln!(stdout, "ERROR\tunknown command")?;
                stdout.flush()?;
            }
        }
    }

    Ok(())
}

fn parse_request(fields: &[&str]) -> Option<ScanRequest> {
    if fields.len() < 5 {
        return None;
    }

    Some(ScanRequest {
        artifact_id: fields[1].to_string(),
        quarantine_path: fields[2].to_string(),
        source_url: fields[3].to_string(),
        suggested_filename: fields[4].to_string(),
    })
}

fn scan_file(request: &ScanRequest) -> Result<ScanReport, String> {
    let data = fs::read(&request.quarantine_path)
        .map_err(|error| format!("failed to read quarantined artifact: {error}"))?;
    let metadata = fs::metadata(&request.quarantine_path)
        .map_err(|error| format!("failed to stat quarantined artifact: {error}"))?;

    let detected_name = if request.suggested_filename.is_empty() {
        Path::new(&request.quarantine_path)
            .file_name()
            .map(|name| name.to_string_lossy().to_string())
            .unwrap_or_else(|| "download.bin".to_string())
    } else {
        request.suggested_filename.clone()
    };

    let extension = Path::new(&detected_name)
        .extension()
        .map(|ext| ext.to_string_lossy().to_ascii_lowercase())
        .unwrap_or_default();
    let magic = detect_magic(&data);
    let mime_guess = mime_from_magic_or_extension(magic, &extension);

    let mut sha256 = Sha256::new();
    sha256.update(&data);
    let sha256 = format!("{:x}", sha256.finalize());

    let mut sha1 = Sha1::new();
    sha1.update(&data);
    let sha1 = format!("{:x}", sha1.finalize());

    let mut heuristics = Vec::new();
    let mut risk_score = 5;

    if metadata.len() == 0 {
      heuristics.push("empty_file".to_string());
      risk_score += 10;
    }

    if request.source_url.starts_with("http://") {
        heuristics.push("insecure_source_transport".to_string());
        risk_score += 10;
    }

    if has_double_extension(&detected_name) {
        heuristics.push("double_extension".to_string());
        risk_score += 20;
    }

    match extension.as_str() {
        "exe" | "dll" | "msi" | "scr" | "com" | "bat" | "cmd" | "ps1" | "sh" | "jar" | "apk"
        | "appimage" => {
            heuristics.push("executable_extension".to_string());
            risk_score += 35;
        }
        "docm" | "xlsm" | "pptm" => {
            heuristics.push("macro_enabled_office_document".to_string());
            risk_score += 35;
        }
        "zip" | "7z" | "rar" | "iso" | "tar" | "gz" | "bz2" => {
            heuristics.push("archive_container".to_string());
            risk_score += 12;
        }
        "svg" | "html" | "htm" | "js" => {
            heuristics.push("active_content_document".to_string());
            risk_score += 18;
        }
        _ => {}
    }

    match magic {
        "pe" | "elf" | "mach-o" | "script" => {
            heuristics.push("executable_magic".to_string());
            risk_score += 35;
        }
        "zip" => {
            heuristics.push("zip_container_magic".to_string());
            risk_score += 10;
        }
        "pdf" => {
            heuristics.push("document_container".to_string());
            risk_score += 4;
        }
        _ => {}
    }

    heuristics.sort();
    heuristics.dedup();

    let risk_level = if risk_score >= 80 {
        "critical"
    } else if risk_score >= 55 {
        "high"
    } else if risk_score >= 25 {
        "medium"
    } else {
        "low"
    }
    .to_string();

    let summary = format!(
        "{} scanned as {} with {} heuristic(s); risk {} ({})",
        detected_name,
        mime_guess,
        heuristics.len(),
        risk_score,
        risk_level
    );

    Ok(ScanReport {
        artifact_id: request.artifact_id.clone(),
        quarantine_path: request.quarantine_path.clone(),
        source_url: request.source_url.clone(),
        suggested_filename: request.suggested_filename.clone(),
        detected_name,
        size_bytes: metadata.len(),
        mime_guess,
        sha256,
        sha1,
        risk_score,
        risk_level,
        heuristics,
        summary,
    })
}

fn detect_magic(data: &[u8]) -> &'static str {
    if data.len() >= 4 && data[0..4] == [0x7f, b'E', b'L', b'F'] {
        "elf"
    } else if data.len() >= 2 && data[0..2] == [b'M', b'Z'] {
        "pe"
    } else if data.len() >= 4 && data[0..4] == [b'P', b'K', 0x03, 0x04] {
        "zip"
    } else if data.len() >= 5 && &data[0..5] == b"%PDF-" {
        "pdf"
    } else if data.len() >= 2 && &data[0..2] == b"#!" {
        "script"
    } else if data.len() >= 4 && &data[0..4] == [0xcf, 0xfa, 0xed, 0xfe] {
        "mach-o"
    } else {
        "unknown"
    }
}

fn mime_from_magic_or_extension(magic: &str, extension: &str) -> String {
    match magic {
        "elf" => "application/x-elf".to_string(),
        "pe" => "application/x-dosexec".to_string(),
        "zip" => "application/zip".to_string(),
        "pdf" => "application/pdf".to_string(),
        "script" => "text/x-shellscript".to_string(),
        "mach-o" => "application/x-mach-binary".to_string(),
        _ => match extension {
            "txt" | "log" | "md" => "text/plain".to_string(),
            "json" => "application/json".to_string(),
            "svg" => "image/svg+xml".to_string(),
            "png" => "image/png".to_string(),
            "jpg" | "jpeg" => "image/jpeg".to_string(),
            "gif" => "image/gif".to_string(),
            "html" | "htm" => "text/html".to_string(),
            "js" => "application/javascript".to_string(),
            "docm" | "docx" => "application/vnd.openxmlformats-officedocument.wordprocessingml.document".to_string(),
            "xlsm" | "xlsx" => "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet".to_string(),
            "pptm" | "pptx" => "application/vnd.openxmlformats-officedocument.presentationml.presentation".to_string(),
            _ => "application/octet-stream".to_string(),
        },
    }
}

fn has_double_extension(filename: &str) -> bool {
    let lowered = filename.to_ascii_lowercase();
    let suspicious_suffixes = [".exe", ".scr", ".js", ".jar", ".sh", ".ps1", ".bat", ".cmd"];
    suspicious_suffixes.iter().any(|suffix| {
        lowered.ends_with(suffix) && lowered[..lowered.len() - suffix.len()].contains('.')
    })
}

fn format_report(report: &ScanReport) -> String {
    [
        "REPORT".to_string(),
        report.artifact_id.clone(),
        report.quarantine_path.clone(),
        report.source_url.clone(),
        report.suggested_filename.clone(),
        report.detected_name.clone(),
        report.size_bytes.to_string(),
        report.mime_guess.clone(),
        report.sha256.clone(),
        report.sha1.clone(),
        report.risk_score.to_string(),
        report.risk_level.clone(),
        report.heuristics.join(","),
        report.summary.clone(),
    ]
    .join("\t")
}
