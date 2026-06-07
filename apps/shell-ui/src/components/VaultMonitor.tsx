import { VaultEvent } from '../types/veyra'

interface Props {
  events: VaultEvent[]
}

const EVENT_ICONS: Record<string, React.ReactNode> = {
  intercepted: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
      <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4" />
      <polyline points="7 10 12 15 17 10" />
      <line x1="12" y1="15" x2="12" y2="3" />
    </svg>
  ),
  'quarantine-routed': (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5" />
    </svg>
  ),
  'quarantine-complete': (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="3" y="3" width="18" height="18" rx="2" ry="2" />
      <line x1="9" y1="9" x2="15" y2="15" />
      <line x1="15" y1="9" x2="9" y2="15" />
    </svg>
  ),
  released: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
      <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14" />
      <polyline points="22 4 12 14.01 9 11.01" />
    </svg>
  ),
  retained: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <rect x="3" y="3" width="18" height="18" rx="2" ry="2" />
      <line x1="12" y1="8" x2="12" y2="16" />
      <line x1="8" y1="12" x2="16" y2="12" />
    </svg>
  ),
  failed: (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="10" />
      <line x1="15" y1="9" x2="9" y2="15" />
      <line x1="9" y1="9" x2="15" y2="15" />
    </svg>
  ),
  'scan-failed': (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z" />
      <line x1="12" y1="9" x2="12" y2="13" />
      <line x1="12" y1="17" x2="12.01" y2="17" />
    </svg>
  ),
  'tool:invoked': (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="3" />
      <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 1 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 1 1-2.83-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 1 1 2.83-2.83l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 1 1 2.83 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z" />
    </svg>
  ),
  'tool:completed': (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <polyline points="20 6 9 17 4 12" />
    </svg>
  ),
  'tool:denied': (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="10" />
      <line x1="15" y1="9" x2="9" y2="15" />
      <line x1="9" y1="9" x2="15" y2="15" />
    </svg>
  ),
  'tool:failed': (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z" />
      <line x1="12" y1="9" x2="12" y2="13" />
      <line x1="12" y1="17" x2="12.01" y2="17" />
    </svg>
  )
}

function getEventIcon(type: string) {
  return EVENT_ICONS[type] ?? (
    <svg className="svg-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
      <circle cx="12" cy="12" r="10" />
      <line x1="12" y1="8" x2="12" y2="12" />
      <line x1="12" y1="16" x2="12.01" y2="16" />
    </svg>
  );
}

function eventColor(type: string): string {
  if (type.includes('released') || type.includes('completed')) return 'green'
  if (type.includes('failed') || type.includes('denied'))      return 'red'
  if (type.includes('retained'))                               return 'yellow'
  if (type.startsWith('tool:'))                                return 'purple'
  return 'blue'
}

function shortType(type: string): string {
  return type.replace('quarantine-', 'q-').replace('tool:', 't:')
}

function eventIdentifier(evt: VaultEvent): string {
  return evt.artifact_id ?? evt.invocation_id ?? evt.tool_id ?? ''
}

function eventDetail(evt: VaultEvent): string {
  if (evt.details) return evt.details
  if (evt.event_type.startsWith('tool:')) {
    const parts: string[] = []
    if (evt.tool_id) parts.push(evt.tool_id)
    if (typeof evt.exit_code === 'number') parts.push(`exit ${evt.exit_code}`)
    if (evt.reason) parts.push(evt.reason)
    if (evt.failure_reason) parts.push(evt.failure_reason)
    return parts.join(' · ') || evt.event_type
  }
  return evt.event_type
}

export default function VaultMonitor({ events }: Props) {
  if (events.length === 0) {
    return (
      <div className="vault-monitor">
        <div className="section-title">BlackVault Events</div>
        <div className="vault-empty card" style={{ padding: '48px 24px', textAlign: 'center' }}>
          <div style={{
            width: 48, height: 48, borderRadius: '50%',
            background: 'rgba(42, 166, 255, 0.05)', border: '1px solid rgba(42, 166, 255, 0.15)',
            display: 'flex', alignItems: 'center', justifyContent: 'center',
            margin: '0 auto 14px', color: 'var(--text-muted)'
          }}>
            <svg style={{ width: 22, height: 22 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              <rect x="3" y="3" width="18" height="18" rx="2" ry="2" />
              <line x1="9" y1="3" x2="9" y2="21" />
            </svg>
          </div>
          <div style={{ fontWeight: 700, fontSize: 13, color: 'var(--text-primary)', marginBottom: 4 }}>Vault Status: Sealed</div>
          <div style={{ color: 'var(--text-muted)', fontSize: 11, maxWidth: 280, margin: '0 auto', lineHeight: 1.5 }}>
            No files or script assets have been quarantined or intercepted during this operational session.
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className="vault-monitor">
      <div className="section-title">Enclave Transaction Log ({events.length})</div>
      <div className="vault-event-list">
        {events.slice(-20).reverse().map((evt, i) => {
          const detail = eventDetail(evt)
          return (
            <div key={i} className="vault-event">
              <div className="vault-event__header">
                <span className={`badge badge--${eventColor(evt.event_type)}`}
                  style={{ fontSize: 10, padding: '2px 8px', display: 'flex', alignItems: 'center', gap: 6 }}>
                  <span style={{ width: 11, height: 11, display: 'inline-flex', alignItems: 'center' }}>{getEventIcon(evt.event_type)}</span>
                  {shortType(evt.event_type).toUpperCase()}
                </span>
                <span className="mono" style={{ color: 'var(--text-muted)', fontSize: 10, fontWeight: 500 }}>
                  {eventIdentifier(evt)}
                </span>
              </div>
              <div className="vault-event__detail" style={{ marginTop: 8, fontSize: 11, fontFamily: 'var(--font-mono)' }}>
                {detail.length > 100 ? detail.slice(0, 100) + '…' : detail}
              </div>
            </div>
          )
        })}
      </div>
    </div>
  )
}
