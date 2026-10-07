use std::collections::HashMap;
use std::env;
use std::io::{self, BufRead, Write};

#[derive(Clone, Debug)]
struct ProfileRecord {
    persona_id: String,
    persona_name: String,
    route_profile_id: String,
    route_profile_name: String,
    route_type: String,
    dns_policy: String,
    webrtc_policy: String,
    leak_prevention_level: String,
    routing_requirement: String,
    hops: Vec<String>,
    notes: String,
}

#[derive(Clone, Debug)]
struct RouteState {
    persona_id: String,
    persona_name: String,
    route_profile_id: String,
    route_profile_name: String,
    route_type: String,
    dns_policy: String,
    webrtc_policy: String,
    leak_prevention_level: String,
    routing_requirement: String,
    health_status: String,
    proxy_uri: String,
    dns_resolver: String,
    leak_status: String,
    diagnostic_summary: String,
    hops: Vec<String>,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 || args[1] != "--stdio" {
        eprintln!("Route engine requires --stdio mode.");
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
    let mut route_states: HashMap<String, RouteState> = HashMap::new();
    let mut buffered_profiles: Vec<ProfileRecord> = Vec::new();
    let mut bootstrap_root = String::new();

    for line_result in stdin.lock().lines() {
        let line = line_result?;
        if line.is_empty() {
            continue;
        }

        let fields: Vec<&str> = line.split('\t').collect();
        match fields.first().copied().unwrap_or_default() {
            "BOOTSTRAP" => {
                buffered_profiles.clear();
                bootstrap_root = fields.get(1).copied().unwrap_or_default().to_string();
            }
            "PROFILE" => {
                if let Some(profile) = parse_profile_record(&fields) {
                    buffered_profiles.push(profile);
                } else {
                    writeln!(stdout, "ERROR\tinvalid profile record")?;
                    stdout.flush()?;
                }
            }
            "SWITCH" => {
                if let Some(profile) = parse_profile_record(&fields) {
                    let route_state = build_route_state(&profile, &bootstrap_root);
                    route_states.insert(profile.persona_id.clone(), route_state.clone());
                    writeln!(stdout, "{}", format_route_line(&route_state))?;
                    writeln!(stdout, "OK\t1")?;
                    stdout.flush()?;
                } else {
                    writeln!(stdout, "ERROR\tinvalid switch record")?;
                    stdout.flush()?;
                }
            }
            "END" => {
                route_states.clear();
                for profile in &buffered_profiles {
                    let route_state = build_route_state(profile, &bootstrap_root);
                    route_states.insert(profile.persona_id.clone(), route_state);
                }

                let mut ordered: Vec<&RouteState> = route_states.values().collect();
                ordered.sort_by(|left, right| left.persona_id.cmp(&right.persona_id));
                for route_state in ordered {
                    writeln!(stdout, "{}", format_route_line(route_state))?;
                }
                writeln!(stdout, "OK\t{}", route_states.len())?;
                stdout.flush()?;
            }
            "STATUS" => {
                let persona_id = fields.get(1).copied().unwrap_or_default();
                if let Some(route_state) = route_states.get(persona_id) {
                    writeln!(stdout, "{}", format_route_line(route_state))?;
                    writeln!(stdout, "OK\t1")?;
                } else {
                    writeln!(stdout, "ERROR\tunknown persona id")?;
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

fn parse_profile_record(fields: &[&str]) -> Option<ProfileRecord> {
    if fields.len() < 11 {
        return None;
    }

    Some(ProfileRecord {
        persona_id: fields[1].to_string(),
        persona_name: fields[2].to_string(),
        route_profile_id: fields[3].to_string(),
        route_profile_name: fields[4].to_string(),
        route_type: fields[5].to_string(),
        dns_policy: fields[6].to_string(),
        webrtc_policy: fields[7].to_string(),
        leak_prevention_level: fields[8].to_string(),
        routing_requirement: fields[9].to_string(),
        hops: fields[10]
            .split(',')
            .filter(|hop| !hop.is_empty())
            .map(|hop| hop.to_string())
            .collect(),
        notes: fields.get(11).copied().unwrap_or_default().to_string(),
    })
}

fn build_route_state(profile: &ProfileRecord, runtime_root: &str) -> RouteState {
    let proxy_uri = match profile.route_type.as_str() {
        "direct" => String::new(),
        "vpn" => "socks5://127.0.0.1:1080".to_string(),
        // Standard local router ports so a real Tor daemon / I2P router works
        // out of the box: system Tor on 9050, I2P HTTP proxy on 4444.
        "tor" => "socks5://127.0.0.1:9050".to_string(),
        "residential_proxy" => "http://127.0.0.1:8080".to_string(),
        "chained" => "socks5://127.0.0.1:9050".to_string(),
        "i2p" => "http://127.0.0.1:4444".to_string(),
        _ => String::new(),
    };

    let dns_resolver = match profile.route_type.as_str() {
        "i2p" => "i2p-proxy-bound".to_string(),
        _ => match profile.dns_policy.as_str() {
            "system" => "system-resolver".to_string(),
            "isolated" => "127.0.0.1:5533".to_string(),
            "proxy_bound" => "127.0.0.1:9053".to_string(),
            _ => "unknown-resolver".to_string(),
        },
    };

    let leak_status = match (profile.leak_prevention_level.as_str(), profile.dns_policy.as_str()) {
        ("maximum", "proxy_bound") => "sealed",
        ("elevated", "isolated") => "guarded",
        ("standard", "system") => "open",
        _ => "guarded",
    }
    .to_string();

    let health_status = if route_matches_expectations(profile) {
        "healthy".to_string()
    } else {
        "degraded".to_string()
    };

    let diagnostic_summary = if health_status == "healthy" {
        format!(
            "{} route staged under {} with {} hop(s).",
            profile.route_profile_name,
            if runtime_root.is_empty() { "default runtime root" } else { runtime_root },
            profile.hops.len()
        )
    } else {
        format!(
            "{} route is missing expected hop structure for {} routing.",
            profile.route_profile_name, profile.route_type
        )
    };

    RouteState {
        persona_id: profile.persona_id.clone(),
        persona_name: profile.persona_name.clone(),
        route_profile_id: profile.route_profile_id.clone(),
        route_profile_name: profile.route_profile_name.clone(),
        route_type: profile.route_type.clone(),
        dns_policy: profile.dns_policy.clone(),
        webrtc_policy: profile.webrtc_policy.clone(),
        leak_prevention_level: profile.leak_prevention_level.clone(),
        routing_requirement: profile.routing_requirement.clone(),
        health_status,
        proxy_uri,
        dns_resolver,
        leak_status,
        diagnostic_summary: if profile.notes.is_empty() {
            diagnostic_summary
        } else {
            format!("{diagnostic_summary} Notes: {}", profile.notes)
        },
        hops: profile.hops.clone(),
    }
}

fn route_matches_expectations(profile: &ProfileRecord) -> bool {
    match profile.route_type.as_str() {
        "direct" => profile.hops.len() >= 2,
        "vpn" => profile.hops.len() >= 2 && profile.hops.iter().any(|hop| hop.contains("vpn")),
        "tor" => profile.hops.len() >= 3 && profile.hops.iter().any(|hop| hop.contains("tor")),
        "residential_proxy" => {
            profile.hops.len() >= 2 && profile.hops.iter().any(|hop| hop.contains("proxy"))
        }
        "chained" => {
            profile.hops.len() >= 3
                && profile.hops.iter().any(|hop| hop.contains("vpn"))
                && profile.hops.iter().any(|hop| hop.contains("tor"))
        }
        "i2p" => {
            profile.hops.len() >= 2 && profile.hops.iter().any(|hop| hop.contains("i2p"))
        }
        _ => false,
    }
}

fn format_route_line(route_state: &RouteState) -> String {
    [
        "ROUTE".to_string(),
        route_state.persona_id.clone(),
        route_state.persona_name.clone(),
        route_state.route_profile_id.clone(),
        route_state.route_profile_name.clone(),
        route_state.route_type.clone(),
        route_state.dns_policy.clone(),
        route_state.webrtc_policy.clone(),
        route_state.leak_prevention_level.clone(),
        route_state.routing_requirement.clone(),
        route_state.health_status.clone(),
        route_state.proxy_uri.clone(),
        route_state.dns_resolver.clone(),
        route_state.leak_status.clone(),
        route_state.diagnostic_summary.clone(),
        route_state.hops.join(","),
    ]
    .join("\t")
}
