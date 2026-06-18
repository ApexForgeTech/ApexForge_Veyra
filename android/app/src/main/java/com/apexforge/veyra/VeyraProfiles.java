package com.apexforge.veyra;

import org.json.JSONArray;
import org.json.JSONObject;

/**
 * Security personas (profiles) for the mobile browser — the same model the
 * desktop shell drives from schemas/seed data, kept in sync here so mobile
 * applies real per-profile security (storage isolation, fingerprint, extension
 * policy, routing intent). Also builds the window.__VEYRA_STATE__ object the
 * React control panel consumes, so the sidebar shows live profile data.
 */
final class VeyraProfiles {

    static final class Persona {
        final String id;
        final String displayName;
        final String securityMode;     // casual | hardened | ghost | redteam
        final String routeProfileId;   // direct_isp | vpn_tunnel | tor_bridge | i2p_network | chained_ops
        final String routeType;        // direct | vpn | tor | i2p | chained
        final String fingerprintId;
        final String extensionPolicyId;
        final boolean ephemeral;       // ghost/red-team → no on-disk trace

        Persona(String id, String displayName, String securityMode, String routeProfileId,
                String routeType, String fingerprintId, String extensionPolicyId, boolean ephemeral) {
            this.id = id;
            this.displayName = displayName;
            this.securityMode = securityMode;
            this.routeProfileId = routeProfileId;
            this.routeType = routeType;
            this.fingerprintId = fingerprintId;
            this.extensionPolicyId = extensionPolicyId;
            this.ephemeral = ephemeral;
        }
    }

    // Mirrors schemas/seed personas. Default route is I2P-capable but starts on
    // the persona's own configured route (changeable in settings).
    static final Persona[] PERSONAS = new Persona[] {
        new Persona("real_identity", "Real Identity", "casual", "direct_isp", "direct",
                "native_stable", "trusted_daily", false),
        new Persona("anonymous", "Anonymous", "ghost", "tor_bridge", "tor",
                "balanced_stealth", "minimal_locked", true),
        new Persona("red_team", "Red Team", "redteam", "chained_ops", "chained",
                "max_entropy", "operator_tools", true),
        new Persona("i2p_research", "I2P Research", "ghost", "i2p_network", "i2p",
                "balanced_stealth", "minimal_locked", true),
    };

    static Persona byId(String id) {
        for (Persona p : PERSONAS) if (p.id.equals(id)) return p;
        return PERSONAS[0];
    }

    /** Builds the full VeyraState JSON the React panel expects. */
    static String buildStateJson(String activeId, String shellVersion) {
        Persona active = byId(activeId);
        try {
            JSONObject root = new JSONObject();

            JSONArray personas = new JSONArray();
            for (Persona p : PERSONAS) personas.put(personaJson(p, p.id.equals(activeId)));
            root.put("active_persona", personaJson(active, true));
            root.put("all_personas", personas);

            JSONObject route = new JSONObject();
            route.put("persona_id", active.id);
            route.put("route_profile_id", active.routeProfileId);
            route.put("route_type", active.routeType);
            route.put("health_status", "direct".equals(active.routeType) ? "healthy" : "requires_router");
            route.put("proxy_uri", proxyFor(active.routeType));
            route.put("dns_resolver", "ghost".equals(active.securityMode) ? "doh_cloudflare" : "system");
            route.put("leak_status", "direct".equals(active.routeType) ? "open" : "guarded");
            route.put("diagnostic_summary",
                    "Mobile session · " + active.routeType + " route · "
                            + (active.ephemeral ? "ephemeral storage" : "persistent vault"));
            root.put("active_route", route);

            JSONArray routeProfiles = new JSONArray();
            routeProfiles.put(routeProfileJson("direct_isp", "Direct ISP", "direct"));
            routeProfiles.put(routeProfileJson("vpn_tunnel", "VPN Tunnel", "vpn"));
            routeProfiles.put(routeProfileJson("tor_bridge", "Tor Bridge", "tor"));
            routeProfiles.put(routeProfileJson("i2p_network", "I2P Network", "i2p"));
            routeProfiles.put(routeProfileJson("chained_ops", "Chained Operations", "chained"));
            root.put("all_route_profiles", routeProfiles);

            root.put("security_mode_id", active.securityMode);
            root.put("security_mode_name", active.securityMode.toUpperCase() + " MODE");
            root.put("fingerprint_profile_id", active.fingerprintId);
            root.put("extension_policy_id", active.extensionPolicyId);
            root.put("allow_eval", "trusted_daily".equals(active.extensionPolicyId));
            root.put("block_mixed_content", !"casual".equals(active.securityMode));
            root.put("blocked_domains_count", "casual".equals(active.securityMode) ? 0 : 1280);

            JSONArray perms = new JSONArray();
            perms.put(permJson("geolocation", active.ephemeral ? "prompt" : "allow"));
            perms.put(permJson("camera", "prompt"));
            perms.put(permJson("microphone", "prompt"));
            perms.put(permJson("notifications", active.ephemeral ? "deny" : "prompt"));
            perms.put(permJson("clipboard", "prompt"));
            root.put("permissions", perms);

            root.put("vault_events", new JSONArray());
            root.put("tools", new JSONArray());

            JSONObject ai = new JSONObject();
            ai.put("id", active.ephemeral ? "ai_restricted" : "ai_scoped");
            ai.put("display_name", active.ephemeral ? "AI Restricted" : "AI Scoped");
            ai.put("enabled", !"red_team".equals(active.id));
            ai.put("model", "local-ollama");
            ai.put("endpoint", "");
            ai.put("allow_page_content", !active.ephemeral);
            ai.put("allow_script_analysis", true);
            ai.put("allow_phishing_check", true);
            ai.put("retain_memory", !active.ephemeral);
            ai.put("max_input_chars", 8000);
            root.put("ai_policy", ai);
            root.put("ai_result", JSONObject.NULL);

            JSONObject osint = new JSONObject();
            osint.put("id", active.ephemeral ? "osint_operational" : "osint_passive");
            osint.put("display_name", "OSINT");
            osint.put("enabled", true);
            osint.put("require_route", !"direct".equals(active.routeType));
            osint.put("allow_whois", true);
            osint.put("allow_dns", true);
            osint.put("allow_archive", true);
            osint.put("allow_username_search", true);
            osint.put("allow_active_probing", "red_team".equals(active.id));
            osint.put("max_username_sites", 30);
            osint.put("retain_cases", !active.ephemeral);
            root.put("osint_policy", osint);

            JSONObject osintCase = new JSONObject();
            osintCase.put("active", false);
            osintCase.put("case_id", "");
            osintCase.put("last_ok", true);
            osintCase.put("last_summary", "");
            osintCase.put("last_error", "");
            osintCase.put("egress", active.routeType);
            osintCase.put("route_type", active.routeType);
            osintCase.put("entities", new JSONArray());
            osintCase.put("relationships", new JSONArray());
            osintCase.put("timeline", new JSONArray());
            root.put("osint_case", osintCase);

            root.put("runtime_root", "/data/data/com.apexforge.veyra");
            root.put("shell_version", shellVersion);
            return root.toString();
        } catch (Exception e) {
            return "{}";
        }
    }

    private static JSONObject personaJson(Persona p, boolean active) throws Exception {
        JSONObject o = new JSONObject();
        o.put("id", p.id);
        o.put("display_name", p.displayName);
        o.put("security_mode", p.securityMode);
        o.put("route_profile_id", p.routeProfileId);
        o.put("fingerprint_profile_id", p.fingerprintId);
        o.put("extension_policy_id", p.extensionPolicyId);
        o.put("ephemeral", p.ephemeral);
        o.put("active", active);
        return o;
    }

    private static JSONObject routeProfileJson(String id, String name, String type) throws Exception {
        JSONObject o = new JSONObject();
        o.put("id", id);
        o.put("display_name", name);
        o.put("route_type", type);
        o.put("dns_policy", "tor".equals(type) || "i2p".equals(type) ? "remote" : "system");
        o.put("leak_prevention_level", "direct".equals(type) ? "standard" : "maximum");
        return o;
    }

    private static JSONObject permJson(String name, String decision) throws Exception {
        JSONObject o = new JSONObject();
        o.put("permission", name);
        o.put("decision", decision);
        o.put("rationale", "Mobile per-persona policy");
        return o;
    }

    private static String proxyFor(String routeType) {
        switch (routeType) {
            case "tor": return "socks5://127.0.0.1:9050";
            case "i2p": return "http://127.0.0.1:4444";
            case "vpn": return "system-vpn";
            case "chained": return "socks5://127.0.0.1:9050";
            default: return "";
        }
    }

    /**
     * Per-persona fingerprint defense injected into every page. Ghost/red-team
     * personas get spoofed navigator surfaces + canvas noise; casual leaves the
     * native fingerprint. Mirrors the desktop fingerprint engine's intent.
     */
    static String fingerprintScript(Persona p) {
        if ("native_stable".equals(p.fingerprintId)) {
            return "";  // casual: no spoofing
        }
        boolean max = "max_entropy".equals(p.fingerprintId);
        int cores = max ? 4 : 8;
        return "(function(){try{"
            + "Object.defineProperty(navigator,'hardwareConcurrency',{get:function(){return " + cores + ";}});"
            + "Object.defineProperty(navigator,'languages',{get:function(){return ['en-US','en'];}});"
            + "Object.defineProperty(navigator,'platform',{get:function(){return 'Linux aarch64';}});"
            + "if(navigator.webdriver===undefined){}else{Object.defineProperty(navigator,'webdriver',{get:function(){return false;}});}"
            // Canvas noise so readback fingerprints differ per session.
            + "var s=" + (max ? "0.06" : "0.02") + ";"
            + "var tdu=HTMLCanvasElement.prototype.toDataURL;"
            + "HTMLCanvasElement.prototype.toDataURL=function(){try{var c=this.getContext('2d');"
            + "if(c){var w=this.width||1,h=this.height||1;var d=c.getImageData(0,0,w,h);"
            + "for(var i=0;i<d.data.length;i+=97){d.data[i]=d.data[i]^(Math.random()<s?1:0);}c.putImageData(d,0,0);}}catch(e){}"
            + "return tdu.apply(this,arguments);};"
            + "}catch(e){}})();";
    }
}
