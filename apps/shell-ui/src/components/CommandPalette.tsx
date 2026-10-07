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
  const trimmedQuery = query.trim()
  const rawCommand = trimmedQuery.startsWith('>') ? trimmedQuery.slice(1).trim() : ''

  const runControl = (command: string) => {
    if (isVeyraHosted()) postVeyraAction({ action: 'control_command', command })
    onClose()
  }

  const commands: Command[] = [
    ...(rawCommand ? [{
      id: `raw:${rawCommand}`,
      label: `Run Desktop Command: ${rawCommand}`,
      description: 'Executes a native browser command such as help, history, click, fill, zoom, or tabs',
      category: 'Raw',
      action: () => runControl(rawCommand),
    }] : []),
    ...state.all_personas.map(p => ({
      id: `persona:${p.id}`,
      label: `Switch Persona -> ${p.display_name}`,
      description: `${p.security_mode.toUpperCase()} | ${p.ephemeral ? 'Ephemeral session' : 'Persistent profile'}`,
      category: 'Persona',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'switch_persona', persona_id: p.id })
        onClose()
      },
    })),
    ...state.all_route_profiles.map(rp => ({
      id: `route:${rp.id}`,
      label: `Switch Route -> ${rp.display_name}`,
      description: `${rp.route_type.toUpperCase()} | ${rp.leak_prevention_level.toUpperCase()} Leak Prevention`,
      category: 'Route',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'switch_route', route_profile_id: rp.id })
        onClose()
      },
    })),
    ...state.tools.filter(t => t.allowed).map(t => ({
      id: `tool:${t.id}`,
      label: `Invoke Tool -> ${t.display_name}`,
      description: `Run diagnostic tool: ${t.id}`,
      category: 'Tool',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'invoke_tool', tool_id: t.id, args: [] })
        onClose()
      },
    })),
    {
      id: 'nav:blank',
      label: 'New Tab',
      description: 'Opens the Veyra start page',
      category: 'Browser',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'open_tab', url: 'veyra:start' })
        onClose()
      },
    },
    { id: 'browser:close-tab', label: 'Close Active Tab', description: 'Closes the currently selected browser tab', category: 'Browser', action: () => runControl('tab-close') },
    { id: 'browser:back', label: 'Go Back', description: 'Navigate the active tab backward', category: 'Browser', action: () => runControl('back') },
    { id: 'browser:forward', label: 'Go Forward', description: 'Navigate the active tab forward', category: 'Browser', action: () => runControl('forward') },
    { id: 'browser:reload', label: 'Reload Page', description: 'Reloads the active tab', category: 'Browser', action: () => runControl('reload') },
    { id: 'browser:find', label: 'Find in Page', description: 'Opens the native find bar', category: 'Browser', action: () => runControl('find') },
    { id: 'browser:site-info', label: 'Site Info', description: 'Opens origin, TLS, permissions, and route details', category: 'Browser', action: () => runControl('site-info') },
    { id: 'browser:settings', label: 'Settings', description: 'Opens Veyra browser settings', category: 'System', action: () => runControl('settings') },
    { id: 'browser:history', label: 'History', description: 'Opens per-persona browsing history', category: 'Browser', action: () => runControl('history') },
    { id: 'browser:history-clear', label: 'Clear Persona History', description: 'Clears history only for the active persona', category: 'System', action: () => runControl('history-clear') },
    { id: 'browser:clear-data', label: 'Clear Active Profile Data', description: 'Clears cookies, cache, and storage for the active desktop profile context', category: 'System', action: () => runControl('clear-data') },
    { id: 'browser:panel-toggle', label: 'Toggle Guard Panel', description: 'Shows or hides the dashboard panel', category: 'System', action: () => runControl('panel toggle') },
    { id: 'browser:zoom-in', label: 'Zoom In', description: 'Increases active page zoom', category: 'Browser', action: () => runControl('zoom in') },
    { id: 'browser:zoom-out', label: 'Zoom Out', description: 'Decreases active page zoom', category: 'Browser', action: () => runControl('zoom out') },
    { id: 'browser:zoom-reset', label: 'Reset Zoom', description: 'Resets active page zoom', category: 'Browser', action: () => runControl('zoom reset') },
    ...(state.ai_policy.enabled && state.ai_policy.allow_page_content ? [{
      id: 'ai:summarize',
      label: 'AI -> Summarize Active Tab',
      description: `Targeting with LLM: ${state.ai_policy.model}`,
      category: 'AI',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'ai_summarize' })
        onClose()
      },
    }] : []),
    ...(state.ai_policy.enabled && state.ai_policy.allow_phishing_check ? [{
      id: 'ai:phishing',
      label: 'AI -> Brand & Phishing Scan',
      description: `Audit current site with heuristics + ${state.ai_policy.model}`,
      category: 'AI',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'ai_phishing_check' })
        onClose()
      },
    }] : []),
    {
      id: 'state:refresh',
      label: 'Refresh Shell State',
      description: 'Synchronizes interface structure from the C++ shell core',
      category: 'System',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'request_state_refresh' })
        onClose()
      },
    },
    ...(state.osint_policy.enabled && state.osint_case.active ? [{
      id: 'osint:clear',
      label: 'OSINT -> Reset Investigation Case',
      description: `Purge all ${state.osint_case.entities.length} loaded entity profiles`,
      category: 'OSINT',
      action: () => {
        if (isVeyraHosted()) postVeyraAction({ action: 'osint_clear' })
        onClose()
      },
    }] : []),
  ]

  const filtered = trimmedQuery
    ? commands.filter(c =>
        c.label.toLowerCase().includes(query.toLowerCase()) ||
        c.description.toLowerCase().includes(query.toLowerCase()) ||
        c.category.toLowerCase().includes(query.toLowerCase()) ||
        c.id.toLowerCase().includes(query.toLowerCase()) ||
        (rawCommand && c.id.startsWith('raw:'))
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
    Persona: 'blue', Route: 'blue', Tool: 'purple', Browser: 'green', System: 'muted', AI: 'purple', OSINT: 'blue', Raw: 'yellow',
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
            placeholder="Search commands, or type > help / > click .button"
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
