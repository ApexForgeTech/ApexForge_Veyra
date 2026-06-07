import { VeyraState, PermissionEntry } from '../types/veyra'

interface Props {
  state: VeyraState
}

const MODE_DESCRIPTIONS: Record<string, string> = {
  casual:   'Standard everyday isolation posture with optimized performance.',
  hardened: 'Strict fingerprint spoofing, cookie containment, and track prevention.',
  ghost:    'Fully ephemeral RAM-only session. Auto-wiped on sandbox reload.',
  redteam:  'Operational isolation posture for adversarial simulation workflows.',
  airgap:   'Restricted file transfer mode with offline staging and metadata scrubbing.',
}

function decisionColor(d: string): string {
  if (d === 'allow')  return 'green'
  if (d === 'prompt') return 'yellow'
  return 'red'
}

function renderDecisionBadge(decision: string) {
  const color = decisionColor(decision);
  const isAllow = decision === 'allow';
  const isPrompt = decision === 'prompt';

  return (
    <span className={`badge badge--${color}`} style={{ fontSize: 10, padding: '2px 8px', display: 'inline-flex', alignItems: 'center', gap: 4 }}>
      {isAllow ? (
        <svg style={{ width: 10, height: 10 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round">
          <polyline points="20 6 9 17 4 12"></polyline>
        </svg>
      ) : isPrompt ? (
        <svg style={{ width: 10, height: 10 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round">
          <circle cx="12" cy="12" r="10"></circle>
          <path d="M9.09 9a3 3 0 0 1 5.83 1c0 2-3 3-3 3"></path>
          <line x1="12" y1="17" x2="12.01" y2="17"></line>
        </svg>
      ) : (
        <svg style={{ width: 10, height: 10 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="3" strokeLinecap="round" strokeLinejoin="round">
          <line x1="18" y1="6" x2="6" y2="18"></line>
          <line x1="6" y1="6" x2="18" y2="18"></line>
        </svg>
      )}
      {decision.toUpperCase()}
    </span>
  );
}

export default function SecurityDashboard({ state }: Props) {
  const modeColor =
    state.security_mode_id === 'casual'   ? 'blue'   :
    state.security_mode_id === 'hardened' ? 'yellow' :
    state.security_mode_id === 'ghost'    ? 'purple' :
    state.security_mode_id === 'redteam'  ? 'red'    : 'red'

  return (
    <div className="security-dashboard">
      <div className="section-title">Security Shield</div>

      <div className="card" style={{ marginBottom: 14 }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 8 }}>
          <span style={{ fontWeight: 800, fontSize: 14, letterSpacing: '0.01em' }}>{state.security_mode_name.toUpperCase()} SHIELD</span>
          <span className={`badge badge--${modeColor}`} style={{ padding: '4px 10px', fontSize: 10 }}>{state.security_mode_id}</span>
        </div>
        <div style={{ fontSize: 11, color: 'var(--text-secondary)', lineHeight: 1.5 }}>
          {MODE_DESCRIPTIONS[state.security_mode_id] ?? state.security_mode_id}
        </div>
      </div>

      <div className="section-title" style={{ marginTop: 16 }}>Operational Permissions</div>
      <div className="permission-grid">
        {state.permissions.map((p: PermissionEntry) => (
          <div key={p.permission} className="permission-row" title={p.rationale} style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
            <span className="permission-name" style={{ fontSize: 11, fontWeight: 600 }}>{p.permission}</span>
            {renderDecisionBadge(p.decision)}
          </div>
        ))}
      </div>

      <div className="divider" style={{ margin: '16px 0' }} />

      <div className="section-title">Ident/Client Fingerprint</div>
      <div className="card" style={{ marginBottom: 14 }}>
        <div className="persona-detail-row" style={{ padding: '2px 0' }}>
          <span className="persona-detail-label">Obfuscation Profile</span>
          <span className="mono" style={{ color: 'var(--accent-bright)', fontWeight: 600 }}>{state.fingerprint_profile_id}</span>
        </div>
      </div>

      <div className="section-title">Sandbox Policies</div>
      <div className="card">
        <div className="persona-detail-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 8, marginBottom: 8 }}>
          <span className="persona-detail-label">Active Sandbox Target</span>
          <span className="mono" style={{ color: 'var(--text-primary)' }}>{state.extension_policy_id}</span>
        </div>
        <div className="security-flag-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 8, marginBottom: 8 }}>
          <span className="security-flag-label" style={{ fontSize: 11, color: 'var(--text-secondary)' }}>JavaScript eval() Execution</span>
          <span className={`badge badge--${state.allow_eval ? 'green' : 'red'}`} style={{ fontSize: 9 }}>
            {state.allow_eval ? 'ALLOWED' : 'BLOCKED'}
          </span>
        </div>
        <div className="security-flag-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 8, marginBottom: 8 }}>
          <span className="security-flag-label" style={{ fontSize: 11, color: 'var(--text-secondary)' }}>Mixed Content Sandbox Block</span>
          <span className={`badge badge--${state.block_mixed_content ? 'red' : 'green'}`} style={{ fontSize: 9 }}>
            {state.block_mixed_content ? 'BLOCKED' : 'ALLOWED'}
          </span>
        </div>
        <div className="security-flag-row" style={{ paddingBottom: 2 }}>
          <span className="security-flag-label" style={{ fontSize: 11, color: 'var(--text-secondary)' }}>DNS Sinkholed Ad/Tracker Rules</span>
          <span className={`badge badge--${state.blocked_domains_count > 0 ? 'yellow' : 'muted'}`} style={{ fontSize: 9 }}>
            {state.blocked_domains_count} BLOCKED
          </span>
        </div>
      </div>
    </div>
  )
}
