import { useState, useEffect, useRef, KeyboardEvent } from 'react'
import { VeyraState, postVeyraAction, isVeyraHosted } from '../types/veyra'

interface Command {
  id: string
  label: string
  description: string
  category: string
  action: () => void
}

interface Props {
  state: VeyraState
  onClose: () => void
}

export default function CommandPalette({ state, onClose }: Props) {
  const [query, setQuery] = useState('')
  const [selected, setSelected] = useState(0)
  const inputRef = useRef<HTMLInputElement>(null)

  const commands: Command[] = [
    ...state.all_route_profiles.map(rp => ({
      id: `route:${rp.id}`,
      label: `Switch Route ➔ ${rp.display_name}`,
      description: `${rp.route_type.toUpperCase()} | ${rp.leak_prevention_level.toUpperCase()} Leak Prevention`,
      category: 'Route',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'switch_route', route_profile_id: rp.id })
        onClose()
      },
    })),
    ...state.tools.filter(t => t.allowed).map(t => ({
      id: `tool:${t.id}`,
      label: `Invoke Binary ➔ ${t.display_name}`,
      description: `Run diagnostic tool: ${t.id}`,
      category: 'Tool',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'invoke_tool', tool_id: t.id, args: [] })
        onClose()
      },
    })),
    {
      id: 'nav:blank',
      label: 'Spawn Clean Sandbox Tab',
      description: 'Opens the Veyra start page',
      category: 'Browser',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'open_tab', url: 'veyra:start' })
        onClose()
      },
    },
    ...(state.ai_policy.enabled && state.ai_policy.allow_page_content ? [{
      id: 'ai:summarize',
      label: 'Cortex AI ➔ Summarize Active Tab',
      description: `Targeting with LLM: ${state.ai_policy.model}`,
      category: 'AI',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'ai_summarize' })
        onClose()
      },
    }] : []),
    ...(state.ai_policy.enabled && state.ai_policy.allow_phishing_check ? [{
      id: 'ai:phishing',
      label: 'Cortex AI ➔ Brand & Phishing Scan',
      description: `Audit current site with heuristics + ${state.ai_policy.model}`,
      category: 'AI',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'ai_phishing_check' })
        onClose()
      },
    }] : []),
    {
      id: 'state:refresh',
      label: 'Daemon ➔ Force Re-read State',
      description: 'Synchronizes interface structure from the C++ shell core',
      category: 'System',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'request_state_refresh' })
        onClose()
      },
    },
    ...(state.osint_policy.enabled && state.osint_case.active ? [{
      id: 'osint:clear',
      label: 'OSINT ➔ Reset Investigation Case',
      description: `Purge all ${state.osint_case.entities.length} loaded entity profiles`,
      category: 'OSINT',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'osint_clear' })
        onClose()
      },
    }] : []),
  ]

  const filtered = query.trim()
    ? commands.filter(c =>
        c.label.toLowerCase().includes(query.toLowerCase()) ||
        c.description.toLowerCase().includes(query.toLowerCase()) ||
        c.category.toLowerCase().includes(query.toLowerCase())
      )
    : commands

  useEffect(() => {
    setSelected(0)
  }, [query])

  useEffect(() => {
    inputRef.current?.focus()
  }, [])

  const handleKey = (e: KeyboardEvent) => {
    if (e.key === 'ArrowDown') {
      e.preventDefault()
      setSelected(s => Math.min(s + 1, filtered.length - 1))
    } else if (e.key === 'ArrowUp') {
      e.preventDefault()
      setSelected(s => Math.max(s - 1, 0))
    } else if (e.key === 'Enter') {
      filtered[selected]?.action()
    } else if (e.key === 'Escape') {
      onClose()
    }
  }

  const CATEGORY_COLORS: Record<string, string> = {
    Route: 'blue', Tool: 'purple', Browser: 'green', System: 'muted', AI: 'purple', OSINT: 'blue',
  }

  return (
    <div className="command-palette-overlay" onClick={onClose}>
      <div className="command-palette" onClick={e => e.stopPropagation()}>
        <div className="command-palette__header">
          <span className="command-palette__icon" style={{ display: 'flex', alignItems: 'center' }}>
            <svg style={{ width: 18, height: 18 }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
              <circle cx="11" cy="11" r="8" />
              <line x1="21" y1="21" x2="16.65" y2="16.65" />
            </svg>
          </span>
          <input
            ref={inputRef}
            className="command-palette__input"
            type="text"
            placeholder="Type a shell command or profile name..."
            value={query}
            onChange={e => setQuery(e.target.value)}
            onKeyDown={handleKey}
          />
          <kbd className="command-palette__esc" onClick={onClose} style={{ display: 'inline-flex', alignItems: 'center', fontSize: 9 }}>ESC</kbd>
        </div>

        <div className="command-palette__results">
          {filtered.length === 0 && (
            <div className="command-palette__empty" style={{ color: 'var(--text-muted)' }}>No matching active commands located.</div>
          )}
          {filtered.map((cmd, i) => (
            <div
              key={cmd.id}
              className={`command-palette__item${i === selected ? ' command-palette__item--selected' : ''}`}
              onClick={cmd.action}
              onMouseEnter={() => setSelected(i)}
              style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}
            >
              <div className="command-palette__item-main">
                <span className="command-palette__item-label" style={{ fontSize: 12.5 }}>{cmd.label}</span>
                <span className="command-palette__item-desc" style={{ fontSize: 10.5 }}>{cmd.description}</span>
              </div>
              <span className={`badge badge--${CATEGORY_COLORS[cmd.category] ?? 'muted'}`}
                style={{ fontSize: 9, padding: '2px 7px' }}>
                {cmd.category.toUpperCase()}
              </span>
            </div>
          ))}
        </div>

        <div className="command-palette__footer" style={{ display: 'flex', gap: 16 }}>
          <span style={{ display: 'inline-flex', alignItems: 'center', gap: 4 }}>
            <kbd style={{ background: 'rgba(255,255,255,0.06)', padding: '1px 4px', borderRadius: 3, border: '1px solid rgba(255,255,255,0.08)' }}>↑↓</kbd> Navigate
          </span>
          <span style={{ display: 'inline-flex', alignItems: 'center', gap: 4 }}>
            <kbd style={{ background: 'rgba(255,255,255,0.06)', padding: '1px 4px', borderRadius: 3, border: '1px solid rgba(255,255,255,0.08)' }}>Enter</kbd> Execute
          </span>
          <span style={{ display: 'inline-flex', alignItems: 'center', gap: 4 }}>
            <kbd style={{ background: 'rgba(255,255,255,0.06)', padding: '1px 4px', borderRadius: 3, border: '1px solid rgba(255,255,255,0.08)' }}>ESC</kbd> Dismiss
          </span>
        </div>
      </div>
    </div>
  )
}
