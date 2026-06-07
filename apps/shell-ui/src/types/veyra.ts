export interface PersonaState {
  id: string;
  display_name: string;
  security_mode: string;
  route_profile_id: string;
  fingerprint_profile_id: string;
  extension_policy_id: string;
  ephemeral: boolean;
  active: boolean;
}

export interface RouteState {
  persona_id: string;
  route_profile_id: string;
  route_type: string;
  health_status: string;
  proxy_uri: string;
  dns_resolver: string;
  leak_status: string;
  diagnostic_summary: string;
}

export interface RouteProfile {
  id: string;
  display_name: string;
  route_type: string;
  dns_policy: string;
  leak_prevention_level: string;
}

export interface PermissionEntry {
  permission: string;
  decision: string;
  rationale: string;
}

export interface VaultEvent {
  event_type: string;
  // Download/artifact events carry these:
  artifact_id?: string;
  tab_id?: string;
  details?: string;
  // Tool-bridge events (event_type starting with "tool:") carry these instead:
  invocation_id?: string;
  tool_id?: string;
  persona_id?: string;
  exit_code?: number;
  reason?: string;
  failure_reason?: string;
}

export interface ToolEntry {
  id: string;
  display_name: string;
  category: string;
  allowed: boolean;
  denial_reason: string;
}

export interface AiPolicy {
  id: string;
  display_name: string;
  enabled: boolean;
  model: string;
  endpoint: string;
  allow_page_content: boolean;
  allow_script_analysis: boolean;
  allow_phishing_check: boolean;
  retain_memory: boolean;
  max_input_chars: number;
}

export interface AiResult {
  method: string;
  text: string;
  risk_level: string;
  risk_score: number;
  source: string;
  ok: boolean;
  error: string;
}

export interface OsintPolicy {
  id: string;
  display_name: string;
  enabled: boolean;
  require_route: boolean;
  allow_whois: boolean;
  allow_dns: boolean;
  allow_archive: boolean;
  allow_username_search: boolean;
  allow_active_probing: boolean;
  max_username_sites: number;
  retain_cases: boolean;
}

export interface OsintEntity {
  type: string;
  value: string;
  source: string;
  attributes: Record<string, unknown>;
}

export interface OsintRelationship {
  from: string;
  to: string;
  kind: string;
}

export interface OsintTimelineEvent {
  ts_ms: number;
  event: string;
  detail: string;
}

export interface OsintCase {
  active: boolean;
  case_id: string;
  last_ok: boolean;
  last_summary: string;
  last_error: string;
  egress: string;
  route_type: string;
  entities: OsintEntity[];
  relationships: OsintRelationship[];
  timeline: OsintTimelineEvent[];
}

export interface VeyraState {
  active_persona: PersonaState;
  all_personas: PersonaState[];
  active_route: RouteState;
  all_route_profiles: RouteProfile[];
  security_mode_id: string;
  security_mode_name: string;
  fingerprint_profile_id: string;
  extension_policy_id: string;
  allow_eval: boolean;
  block_mixed_content: boolean;
  blocked_domains_count: number;
  permissions: PermissionEntry[];
  vault_events: VaultEvent[];
  tools: ToolEntry[];
  ai_policy: AiPolicy;
  ai_result: AiResult | null;
  osint_policy: OsintPolicy;
  osint_case: OsintCase;
  runtime_root: string;
  shell_version: string;
}

// Global bridge declared by C++ shell at document-start via user script
declare global {
  interface Window {
    __VEYRA_STATE__: VeyraState | undefined;
    __VEYRA_UPDATE__: ((state: VeyraState) => void) | undefined;
    webkit?: {
      messageHandlers?: {
        veyra?: {
          postMessage: (message: VeyraAction) => void;
        };
      };
    };
  }
}

export type VeyraAction =
  | { action: 'switch_route'; route_profile_id: string }
  | { action: 'invoke_tool'; tool_id: string; args: string[] }
  | { action: 'navigate'; url: string }
  | { action: 'open_tab'; url: string }
  | { action: 'request_state_refresh' }
  | { action: 'ai_summarize' }
  | { action: 'ai_phishing_check' }
  | { action: 'ai_explain_script'; payload: string }
  | { action: 'osint_whois'; target: string }
  | { action: 'osint_dns'; target: string }
  | { action: 'osint_archive'; target: string }
  | { action: 'osint_username'; target: string }
  | { action: 'osint_clear' };

export function postVeyraAction(action: VeyraAction): void {
  window.webkit?.messageHandlers?.veyra?.postMessage(action);
}

export function isVeyraHosted(): boolean {
  return typeof window.webkit?.messageHandlers?.veyra !== 'undefined';
}
