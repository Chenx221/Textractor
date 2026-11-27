#pragma once

// engine/v8/v8_bootstrap.h
//
// The JavaScript that is compiled and run inside the live renderer.  It is
// shared by every V8-based engine (NW.js, Electron, ...):
//
//   kBootstrapCore       named-pipe transport + engine registry
//   kBootstrapRpgMaker   RPG Maker MZ / MV text hooks
//   kBootstrapTyrano     TyranoScript text hooks
//   kBootstrapTail       starts the engine pump (must stay last)
//

inline constexpr char kBootstrapCore[] = R"JS(
(function () {
    var pipe = __TT_PIPE;
    var existing = window.__ttNwjsV8;

    /* Textractor was detached and injected again: the previous socket belonged
       to a DLL that is now unloaded. Point the old engine hooks at the new
       pipe instead of installing a second set of hooks. */
    if (existing && existing.alive) { existing.retarget(pipe); return 'retarget'; }

    var net = null;
    try { net = require('net'); } catch (e) { net = null; }

    var api = window.__ttNwjsV8 = {
        alive: true, sent: 0, engines: [], errors: [], last: '',
        pipe: pipe, sock: null, queue: [], fails: 0, pipeMode: !!net
    };

    /* All transport state lives on api on purpose: the hooks installed by the
       first injection keep calling api.send, which reads api.sock, so a later
       re-attach only has to retarget. */
    api.connect = function () {
        if (!net || api.sock) return;
        try {
            var s = net.connect(api.pipe);
            api.sock = s;
            api.connectAt = Date.now();
            s.on('connect', function () {
                api.fails = 0;
                if (api.errors.length < 20) api.errors.push('connected');
                while (api.queue.length) s.write(api.queue.shift());
            });
            s.on('error', function (e) {
                if (api.errors.length < 20) api.errors.push('error: ' + (e && e.message));
                retry(s);
            });
            s.on('close', function () { retry(s); });
        } catch (e) { api.errors.push('connect: ' + e); }
    };

    function retry(s) {
        if (api.sock === s) api.sock = null;
        if (++api.fails < 30) setTimeout(api.connect, 2000);  /* host gone: give up */
    }

    /* A pending connect can stay pending forever: libuv reports neither
       connect nor error when the pipe appears late or the host goes away
       mid-handshake. Recycle it instead of wedging the transport. */
    if (net) setInterval(function () {
        if (api.sock && api.sock.connecting && Date.now() - api.connectAt > 3000) {
            api.fails++;
            if (api.errors.length < 20) api.errors.push('connect timeout #' + api.fails);
            try { api.sock.destroy(); } catch (e) { }
            api.sock = null;
            api.connect();
        }
    }, 1500);

    api.send = function (text, speaker) {
        var line = JSON.stringify({ s: speaker || '', t: String(text) }) + '\n';
        api.sent++;
        if (api.sock && api.sock.writable) api.sock.write(line);
        else if (api.queue.length < 200) api.queue.push(line);
    };

    api.drain = function () {
        if (api.pipeMode || !api.queue.length) return '';
        var s = api.queue.join('\n');
        api.queue.length = 0;
        return s;
    };

    api.retarget = function (p) {
        if (!net) return;
        api.pipe = p;
        try { if (api.sock) api.sock.destroy(); } catch (e) {}
        api.sock = null;
        api.fails = 0;
        api.connect();
    };

    api.connect();

    /* Engine registry: detect() is polled until it returns true, install()
       then runs exactly once. Games load their scripts asynchronously, so
       polling is required -- the first JS call we hook may happen before
       rmmz_core.js exists. */
    var engines = [];
    api.registerEngine = function (name, detect, install) {
        engines.push({ name: name, detect: detect, install: install, done: false });
    };
    api.pumpEngines = function () {
        for (var i = 0; i < engines.length; i++) {
            var e = engines[i];
            if (e.done) continue;
            var ready = false;
            try { ready = !!e.detect(); } catch (x) { ready = false; }
            if (!ready) continue;
            e.done = true;
            try { e.install(); api.engines.push(e.name); }
            catch (x) { api.errors.push(e.name + ': ' + x); }
        }
    };
)JS";

inline constexpr char kBootstrapRpgMaker[] = R"JS(
    /* A control code is a backslash, a code name, and an optional bracketed
       parameter. The parameter is NOT always a number: this game uses
       \SE[12-nb] (sound file), \SM[rock], \SP[normal2 5], and MZ only parses
       digits, which is why taking "\[[0-9]+\]" left the "[12-nb]" behind. */
    var RM_CODES = /\x1b([A-Za-z]+|[\$.|^!><{}\\])(\[[^\]]*\])?/g;

    function rmExpand(input) {
        if (typeof input !== 'string' || !input.length) return '';
        var text = input;
        /* a literal backslash is "\\", every other backslash starts a code */
        text = text.replace(/\\/g, '\x1b').replace(/\x1b\x1b/g, '\\');

        /* \V[n] can expand to text that contains more codes: loop like MZ */
        for (var guard = 0; guard < 8 && /\x1b[vV]\[(\d+)\]/.test(text); guard++)
            text = text.replace(/\x1b[vV]\[(\d+)\]/g, function (m, n) {
                try {
                    var v = $gameVariables.value(parseInt(n, 10));
                    return (v === undefined || v === null) ? '' : String(v);
                } catch (e) { return ''; }
            });

        text = text.replace(/\x1b[nN]\[(\d+)\]/g, function (m, n) {
            try { var a = $gameActors.actor(parseInt(n, 10)); return a ? a.name() : ''; }
            catch (e) { return ''; }
        });
        text = text.replace(/\x1b[pP]\[(\d+)\]/g, function (m, n) {
            try { var a = $gameParty.members()[parseInt(n, 10) - 1]; return a ? a.name() : ''; }
            catch (e) { return ''; }
        });
        text = text.replace(/\x1b[gG]/g, function () {
            try { return TextManager.currencyUnit; } catch (e) { return ''; }
        });

        /* \C[n] \I[n] \PX[n] \PY[n] \FS[n] and \{ \} \$ \. \| \! \> \< \^
           only affect layout: drop the code and its parameter, keep the text */
        text = text.replace(RM_CODES, function (m, code) {
            return code === '\\' ? '\\' : '';
        });
        return text.replace(/\x1b/g, '');
    }

    window.__ttNwjsV8.registerEngine('rpgmaker',
        function () {
            if (typeof Game_Interpreter === 'undefined' || typeof SceneManager === 'undefined') return false;
            if (!Game_Interpreter.prototype || typeof Game_Interpreter.prototype.command101 !== 'function') return false;
            var name = (typeof Utils !== 'undefined' && Utils.RPGMAKER_NAME) || '';
            return name === 'MZ' || name === 'MV' || name === '';   /* '' = RM shaped, version unknown */
        },
        function () {
            var lastKey = '';

            function emit(text, speaker) {
                var t = rmExpand(text);
                if (!t) return;
                var sp = rmExpand(speaker === undefined || speaker === null ? '' : String(speaker)).trim();
                if (sp === 'undefined' || sp === 'null') sp = '';
                var key = sp + '\u0000' + t;
                if (key === lastKey) return;   /* same window repainted */
                lastKey = key;
                api.last = t;
                api.send(t, sp);   /* via api so a re-attach retargets the socket */
            }

            function hook(proto, name, make) {
                if (!proto || typeof proto[name] !== 'function') return false;
                proto[name] = make(proto[name]);
                return true;
            }

            var in101 = false;
            hook(Game_Interpreter.prototype, 'command101', function (orig) {
                return function () {
                    var r;
                    in101 = true;
                    try { r = orig.apply(this, arguments); } finally { in101 = false; }
                    try {
                        if (typeof $gameMessage !== 'undefined' && $gameMessage &&
                            Array.isArray($gameMessage._texts) && $gameMessage._texts.length)
                            emit($gameMessage._texts.join('\n'), $gameMessage._speakerName);
                    } catch (e) { api.errors.push('101: ' + e); }
                    return r;
                };
            });

            hook(Game_Interpreter.prototype, 'command105', function (orig) {
                return function (params) {
                    var r = orig.apply(this, arguments);
                    try { if (Array.isArray(params) && params.length > 1) emit(params.slice(1).join('\n'), ''); }
                    catch (e) { api.errors.push('105: ' + e); }
                    return r;
                };
            });

            hook(Game_Message.prototype, 'add', function (orig) {
                return function (text) {
                    var r = orig.apply(this, arguments);
                    if (!in101) { try { emit(String(text), this._speakerName); } catch (e) { api.errors.push('add: ' + e); } }
                    return r;
                };
            });
        });
)JS";

inline constexpr char kBootstrapTyrano[] = R"JS(
    /* ------------------------------------------------------------------ *
     *  TyranoScript (Electron / NW.js builds)
     * ------------------------------------------------------------------ */
    window.__ttNwjsV8.registerEngine('tyrano',
        function () {
            return typeof tyrano !== 'undefined' && tyrano.plugin && tyrano.plugin.kag &&
                   tyrano.plugin.kag.tag && tyrano.plugin.kag.tag.text &&
                   typeof tyrano.plugin.kag.tag.text.showMessage === 'function';
        },
        function () {
            var tag = tyrano.plugin.kag.tag;

            function clean(x) {
                return String(x || '')
                    .replace(/<[^>]*>/g, '')
                    .replace(/\[[a-zA-Z_]+[^\]]*\]/g, '')
                    .replace(/&nbsp;/g, ' ')
                    .trim();
            }

            var lastKey = '';
            function emit(text, name, kind) {
                var t = clean(text);
                if (!t) return;
                var sp = clean(name);
                var key = sp + '\u0000' + t;
                if (key === lastKey) return;
                lastKey = key;
                api.last = t;
                api.send(t, sp);
            }

            /* text tag: merge consecutive text tags, the same way the game
               concatenates them before drawing a single message window */
            var pendingBreak = false;

            if (!tag.text.__ttHook) {
                tag.text.__ttHook = true;
                var origShowMessage = tag.text.showMessage;
                var sentence = '', needCombine = false;

                tag.text.showMessage = function (message_str, pm, isVertical) {
                    try {
                        pm = pm || {};
                        var val = (pm.val !== undefined) ? String(pm.val) : String(message_str);
                        sentence = needCombine ? (sentence + (pendingBreak ? '\n' : '') + val) : val;
                        pendingBreak = false;

                        var combineNext = false;
                        try {
                            var ftag = this.kag && this.kag.ftag;
                            if (ftag && Array.isArray(ftag.array_tag)) {
                                var idx = ftag.current_order_index + 1;
                                var next = ftag.array_tag[idx] ? ftag.array_tag[idx].name : '';
                                var next2 = ftag.array_tag[idx + 1] ? ftag.array_tag[idx + 1].name : '';
                                if (next === 'text' || next === 'wait' || next === 'r' ||
                                    next === 'l' || next === 'Hxn' || next === 'endmacro' ||
                                    next === 'ruby' || next === 'glink' || next === 'resetdelay' ||
                                    next === 'delay') {
                                    combineNext = true;
                                }
                                if (next2 === '_tb_end_text') combineNext = false;
                            }
                        } catch (e) { }
                        needCombine = combineNext;

                        if (!needCombine) {
                            var name = '';
                            try {
                                var ptext = this.kag && this.kag.stat && this.kag.stat.chara_ptext;
                                if (ptext && typeof $ !== 'undefined') {
                                    var el = $('.' + ptext);
                                    if (el.length > 0) {
                                        var raw = el.html();
                                        if (raw && String(raw).trim() !== '' &&
                                            raw !== 'null' && raw !== 'undefined') {
                                            name = el.text().trim();
                                        }
                                    }
                                }
                            } catch (e) { }
                            emit(sentence, name, 'text');
                            sentence = '';
                        }
                    } catch (e) { api.errors.push('tyrano.text:' + e); }
                    return origShowMessage.apply(this, arguments);
                };
            }

            function hookBreak(name) {
                var t = tag[name];
                if (!t || typeof t.start !== 'function' || t.__ttBreak) return;
                t.__ttBreak = true;
                var origStart = t.start;
                t.start = function () {
                    pendingBreak = true;
                    return origStart.apply(this, arguments);
                };
            }
            hookBreak('r');
            hookBreak('l');

            /* menu style choices */
            if (tag.glink && typeof tag.glink.start === 'function' && !tag.glink.__ttHook) {
                tag.glink.__ttHook = true;
                var origGlink = tag.glink.start;
                var pendingG = [], timerG = null;
                function flushG() {
                    if (pendingG.length) emit(pendingG.join('\n'), '', 'choice');
                    pendingG = []; timerG = null;
                }
                tag.glink.start = function (pm) {
                    try {
                        if (pm && typeof pm.text === 'string' && pm.text.trim()) {
                            pendingG.push(pm.text.trim());
                            if (timerG) clearTimeout(timerG);
                            timerG = setTimeout(flushG, 150);
                        }
                    } catch (e) { api.errors.push('tyrano.glink:' + e); }
                    return origGlink.apply(this, arguments);
                };
            }

            /* inline choices ([link] inside the message) */
            if (tag.link && typeof tag.link.start === 'function' && !tag.link.__ttHook) {
                tag.link.__ttHook = true;
                var origLink = tag.link.start;
                var pendingL = [], timerL = null;
                function flushL() {
                    if (pendingL.length) emit(pendingL.join('\n'), '', 'choice');
                    pendingL = []; timerL = null;
                }
                tag.link.start = function (pm) {
                    try {
                        var txt = pm && (pm.text || pm.title);
                        if (typeof txt === 'string' && txt.trim()) {
                            pendingL.push(txt.trim());
                            if (timerL) clearTimeout(timerL);
                            timerL = setTimeout(flushL, 200);
                        }
                    } catch (e) { api.errors.push('tyrano.link:' + e); }
                    return origLink.apply(this, arguments);
                };
            }
        });
)JS";

inline constexpr char kBootstrapTail[] = R"JS(
    setInterval(function () { try { api.pumpEngines(); } catch (e) {} }, 500);
    api.pumpEngines();
    return 'ok';
})()
)JS";

