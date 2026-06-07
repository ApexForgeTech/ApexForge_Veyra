import { useState } from 'react'
import { ToolEntry, postVeyraAction, isVeyraHosted } from '../types/veyra'

interface Props {
  tools: ToolEntry[]
}

const CATEGORY_ICONS: Record<string, React.ReactNode> = {
  network_scan: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="2" y="2" width="20" height="8" rx="2" ry="2" />
      <rect x="2" y="14" width="20" height="8" rx="2" ry="2" />
      <line x1="6" y1="6" x2="6.01" y2="6" />
      <line x1="6" y1="18" x2="6.01" y2="18" />
    </svg>
  ),
  osint: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="11" cy="11" r="8" />
      <line x1="21" y1="21" x2="16.65" y2="16.65" />
    </svg>
  ),
  web: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M18 13v6a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V8a2 2 0 0 1 2-2h6" />
      <polyline points="15 3 21 3 21 9" />
      <line x1="10" y1="14" x2="21" y2="3" />
    </svg>
  ),
  crypto: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="3" y="11" width="18" height="11" rx="2" ry="2" />
      <path d="M7 11V7a5 5 0 0 1 10 0v4" />
    </svg>
  ),
  forensics: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
      <polyline points="14 2 14 8 20 8" />
      <line x1="16" y1="13" x2="8" y2="13" />
      <line x1="16" y1="17" x2="8" y2="17" />
      <polyline points="10 9 9 9 8 9" />
    </svg>
  ),
  general: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="3" />
      <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z" />
    </svg>
  ),
}

function getCategoryIcon(toolId: string) {
  if (toolId.includes('nmap')) return CATEGORY_ICONS.network_scan;
  if (toolId.includes('whois') || toolId.includes('dig')) return CATEGORY_ICONS.osint;
  if (toolId.includes('curl')) return CATEGORY_ICONS.web;
  if (toolId.includes('openssl')) return CATEGORY_ICONS.crypto;
  if (toolId.includes('strings') || toolId.includes('file')) return CATEGORY_ICONS.forensics;
  return CATEGORY_ICONS.general;
}

export default function ToolLauncher({ tools }: Props) {
  const [expanded, setExpanded] = useState<string | null>(null)
  const [argsInput, setArgsInput] = useState('')
  const [running, setRunning] = useState<string | null>(null)

  const handleInvoke = (toolId: string) => {
    if (!isVeyraHosted()) return
    const args = argsInput.trim().split(/\s+/).filter(Boolean)
    setRunning(toolId)
    postVeyraAction({ action: 'invoke_tool', tool_id: toolId, args })
    setTimeout(() => {
      setRunning(null)
      setExpanded(null)
      setArgsInput('')
    }, 1500)
  }

  return (
    <div className="tool-launcher">
      <div className="section-title">Available Binaries</div>
      <div className="tool-list">
        {tools.map(tool => {
          const isExpanded = expanded === tool.id;
          return (
            <div key={tool.id} className={`tool-row${!tool.allowed ? ' tool-row--denied' : ''}`}>
              <div
                className="tool-row__header"
                onClick={() => tool.allowed && setExpanded(isExpanded ? null : tool.id)}
                style={{ display: 'flex', alignItems: 'center' }}
              >
                <span className="tool-category-icon" style={{ display: 'flex', alignItems: 'center', width: 20, height: 20, color: 'var(--accent-bright)' }}>
                  {getCategoryIcon(tool.id)}
                </span>
                <div className="tool-info" style={{ marginLeft: 6 }}>
                  <span className="tool-name">{tool.display_name}</span>
                  <span className="tool-id mono">{tool.id}</span>
                </div>
                <span className={`badge badge--${tool.allowed ? 'green' : 'red'}`}
                  style={{ fontSize: 9 }}>
                  {tool.allowed ? '✓ ALLOWED' : '✗ RESTRICTED'}
                </span>
              </div>

              {isExpanded && tool.allowed && (
                <div className="tool-expand">
                  {tool.denial_reason && (
                    <div className="tool-warning">{tool.denial_reason}</div>
                  )}
                  <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                    <div style={{ fontSize: 9, fontFamily: 'var(--font-mono)', color: 'var(--text-muted)' }}>
                      $ {tool.id} [arguments]
                    </div>
                    <input
                      className="tool-args-input"
                      type="text"
                      placeholder="e.g. -v -A -T4 127.0.0.1"
                      value={argsInput}
                      onChange={e => setArgsInput(e.target.value)}
                      onKeyDown={e => e.key === 'Enter' && handleInvoke(tool.id)}
                      autoFocus
                    />
                    <button
                      className="btn btn--primary"
                      style={{ width: '100%', justifyContent: 'center', marginTop: 4 }}
                      onClick={() => handleInvoke(tool.id)}
                      disabled={running === tool.id}
                    >
                      {running === tool.id ? (
                        <>
                          <svg className="shimmer" style={{ width: 14, height: 14, animation: 'logoGlow 1s infinite alternate', marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                            <path d="M21.5 2v6h-6M21.34 15.57a10 10 0 1 1-.57-8.38l5.67-5.67" />
                          </svg>
                          Executing Process...
                        </>
                      ) : (
                        <>
                          <svg style={{ width: 14, height: 14, marginRight: 6 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                            <polygon points="5 3 19 12 5 21 5 3"></polygon>
                          </svg>
                          Execute Binary
                        </>
                      )}
                    </button>
                  </div>
                </div>
              )}

              {!tool.allowed && tool.denial_reason && (
                <div className="tool-denial-reason">{tool.denial_reason}</div>
              )}
            </div>
          );
        })}
      </div>
    </div>
  )
}
