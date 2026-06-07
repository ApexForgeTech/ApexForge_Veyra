import { useState } from 'react'
import { AiPolicy, AiResult, postVeyraAction, isVeyraHosted } from '../types/veyra'

interface Props {
  policy: AiPolicy
  result: AiResult | null
}

function riskColor(level: string): string {
  if (level === 'high') return 'red'
  if (level === 'medium') return 'yellow'
  if (level === 'low') return 'yellow'
  if (level === 'clean') return 'green'
  return 'muted'
}

function sourceBadge(source: string): string {
  if (source === 'model') return 'purple'
  if (source === 'heuristic') return 'blue'
  return 'muted'
}

const METHOD_LABELS: Record<string, string> = {
  summarize:      'Page Briefing & Analysis',
  phishing_check: 'Anti-Phishing Scan Result',
  explain_script: 'Deobfuscated Script Analysis',
  ping:           'LLM Operational Status',
}

export default function AiPanel({ policy, result }: Props) {
  const [scriptInput, setScriptInput] = useState('')
  const [pending, setPending] = useState<string | null>(null)

  const hosted = isVeyraHosted()

  const run = (kind: 'summarize' | 'phishing' | 'script') => {
    if (!hosted) return
    setPending(kind)
    if (kind === 'summarize') postVeyraAction({ action: 'ai_summarize' })
    else if (kind === 'phishing') postVeyraAction({ action: 'ai_phishing_check' })
    else postVeyraAction({ action: 'ai_explain_script', payload: scriptInput })
    setTimeout(() => setPending(null), 4000)
  }

  if (!policy.enabled) {
    return (
      <div className="ai-panel">
        <div className="section-title">Cortex AI Status</div>
        <div className="ai-disabled card" style={{ padding: '40px 20px', textAlign: 'center' }}>
          <div style={{
            width: 48, height: 48, borderRadius: '50%',
            background: 'rgba(255, 59, 82, 0.05)', border: '1px solid rgba(255, 59, 82, 0.15)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            margin: '0 auto 14px', color: 'var(--danger)'
          }}>
            <svg style={{ width: 22, height: 22 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <rect x="3" y="11" width="18" height="11" rx="2" ry="2" />
              <path d="M7 11V7a5 5 0 0 1 10 0v4" />
            </svg>
          </div>
          <div style={{ fontWeight: 700, fontSize: 13, color: 'var(--text-primary)', marginBottom: 4 }}>AI Co-Processor Off</div>
          <div style={{ color: 'var(--text-muted)', fontSize: 11, maxWidth: 280, margin: '0 auto', lineHeight: 1.5 }}>
            The active persona's AI policy (<span className="mono">{policy.id}</span>) enforces local processing only. Sandbox telemetry isolation active.
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className="ai-panel">
      <div className="section-title">Active AI Policy</div>

      {/* Policy card */}
      <div className="card" style={{ marginBottom: 12 }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: 10 }}>
          <span style={{ fontWeight: 800, color: 'var(--purple-bright)', fontSize: 14 }}>{policy.display_name.toUpperCase()}</span>
          <span className="badge badge--purple">{policy.model}</span>
        </div>
        <div className="ai-cap-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 6, marginBottom: 6 }}>
          <span className="ai-cap-label">Telemetry Gathering</span>
          <span className={`badge badge--${policy.allow_page_content ? 'green' : 'red'}`} style={{ fontSize: 9 }}>
            {policy.allow_page_content ? 'ENABLED' : 'BLOCKED'}
          </span>
        </div>
        <div className="ai-cap-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 6, marginBottom: 6 }}>
          <span className="ai-cap-label">Script Sandboxed Analysis</span>
          <span className={`badge badge--${policy.allow_script_analysis ? 'green' : 'red'}`} style={{ fontSize: 9 }}>
            {policy.allow_script_analysis ? 'ENABLED' : 'BLOCKED'}
          </span>
        </div>
        <div className="ai-cap-row" style={{ borderBottom: '1px solid rgba(42, 166, 255, 0.05)', paddingBottom: 6, marginBottom: 6 }}>
          <span className="ai-cap-label">Memory Retention Policy</span>
          <span className={`badge badge--${policy.retain_memory ? 'yellow' : 'muted'}`} style={{ fontSize: 9 }}>
            {policy.retain_memory ? 'RETAINED' : 'EPHEMERAL'}
          </span>
        </div>
        <div className="ai-cap-row" style={{ paddingBottom: 2 }}>
          <span className="ai-cap-label">Target Endpoint Node</span>
          <span className="mono" style={{ fontSize: 10, color: 'var(--text-secondary)' }}>{policy.endpoint}</span>
        </div>
      </div>

      {/* Actions */}
      <div className="section-title" style={{ marginTop: 16 }}>Operational Actions</div>
      <div className="ai-actions">
        <button
          className="btn btn--primary ai-action-btn"
          onClick={() => run('summarize')}
          disabled={!hosted || !policy.allow_page_content || pending === 'summarize'}
          title={policy.allow_page_content ? 'Summarize the active tab' : 'Page content blocked by policy'}
        >
          {pending === 'summarize' ? (
            <>
              <svg className="shimmer" style={{ width: 14, height: 14, animation: 'logoGlow 1s infinite alternate', marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67" />
              </svg>
              Generating Briefing...
            </>
          ) : (
            <>
              <svg style={{ width: 14, height: 14, marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"></path>
                <polyline points="14 2 14 8 20 8"></polyline>
                <line x1="16" y1="13" x2="8" y2="13"></line>
                <line x1="16" y1="17" x2="8" y2="17"></line>
                <polyline points="10 9 9 9 8 9"></polyline>
              </svg>
              Briefing / Summarize Page
            </>
          )}
        </button>
        <button
          className="btn btn--ghost ai-action-btn"
          onClick={() => run('phishing')}
          disabled={!hosted || !policy.allow_phishing_check || pending === 'phishing'}
          style={{ border: '1px solid rgba(255, 176, 32, 0.2)', color: 'var(--warning)' }}
        >
          {pending === 'phishing' ? (
            <>
              <svg className="shimmer" style={{ width: 14, height: 14, animation: 'logoGlow 1s infinite alternate', marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67" />
              </svg>
              Analyzing Brand Telemetry...
            </>
          ) : (
            <>
              <svg style={{ width: 14, height: 14, marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"></path>
                <line x1="12" y1="9" x2="12" y2="13"></line>
                <line x1="12" y1="17" x2="12.01" y2="17"></line>
              </svg>
              Scan Phishing Risks
            </>
          )}
        </button>
      </div>

      {/* Script explanation */}
      {policy.allow_script_analysis && (
        <div className="ai-script-section" style={{ marginTop: 16 }}>
          <div className="section-title">Analyze Script Telemetry</div>
          <textarea
            className="ai-script-input"
            placeholder="Paste raw JavaScript code for structural entropy, obfuscation check, or network leakage review..."
            value={scriptInput}
            onChange={e => setScriptInput(e.target.value)}
            rows={4}
            style={{ width: '100%', padding: 12, borderRadius: 'var(--radius-md)', background: 'rgba(2, 6, 12, 0.4)', border: '1px solid var(--border)' }}
          />
          <button
            className="btn btn--primary"
            style={{ width: '100%', justifyContent: 'center', marginTop: 8 }}
            onClick={() => run('script')}
            disabled={!hosted || !scriptInput.trim() || pending === 'script'}
          >
            {pending === 'script' ? (
              <>
                <svg className="shimmer" style={{ width: 14, height: 14, animation: 'logoGlow 1s infinite alternate', marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                  <path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67" />
                </svg>
                Running Structural Auditing...
              </>
            ) : (
              <>
                <svg style={{ width: 14, height: 14, marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.2" strokeLinecap="round" strokeLinejoin="round">
                  <circle cx="12" cy="12" r="10"></circle>
                  <line x1="12" y1="16" x2="12" y2="12"></line>
                  <line x1="12" y1="8" x2="12.01" y2="8"></line>
                </svg>
                Audit Code Snippet
              </>
            )}
          </button>
        </div>
      )}

      {/* Result */}
      {result && (
        <div className="ai-result" style={{ marginTop: 18 }}>
          <div className="section-title">
            {METHOD_LABELS[result.method] ?? 'Audit Logs'}
          </div>
          <div className={`card ai-result-card${result.ok ? '' : ' ai-result-card--error'}`} style={{ marginTop: 6, padding: '16px' }}>
            <div className="ai-result-header" style={{ marginBottom: 12 }}>
              {result.method === 'phishing_check' && result.risk_level && (
                <span className={`badge badge--${riskColor(result.risk_level)}`} style={{ padding: '3px 10px', fontSize: 10 }}>
                  RISK POSTURE: {result.risk_level.toUpperCase()} ({result.risk_score}/100)
                </span>
              )}
              {result.source && (
                <span className={`badge badge--${sourceBadge(result.source)}`} style={{ fontSize: 9 }}>
                  CORE: {result.source.toUpperCase()}
                </span>
              )}
              {!result.ok && (
                <span className="badge badge--red" style={{ fontSize: 9 }}>ERR LOG</span>
              )}
            </div>
            <div className="ai-result-text" style={{ fontSize: 12, lineHeight: 1.6, color: 'var(--text-primary)', fontFamily: 'var(--font-mono)' }}>
              {result.ok ? result.text : (result.error || result.text)}
            </div>
          </div>
        </div>
      )}

      {!hosted && (
        <div style={{ marginTop: 16, fontSize: 11, color: 'var(--text-muted)', textAlign: 'center', lineHeight: 1.5 }}>
          AI Telemetry requires integration with the C++ host daemon.
        </div>
      )}
    </div>
  )
}
