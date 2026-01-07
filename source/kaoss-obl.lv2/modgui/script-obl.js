function (event) {

    const STATE_NAMES = [
        "∅ EMPTY",
        "… READY",
        "⏺ RECORD",
        "⏹ STOP",
        "▶ PLAY",
        "⊕ OVERDUB"
    ];

    const STATE_CLASSES = [
        "state-empty",
        "state-waiting",
        "state-recording",
        "state-stopped",
        "state-playing",
        "state-overdubbing"
    ];

    function handle_event(symbol, value) {
        const display = event.icon.find('[mod-role=looper-display]');
        const stateText = event.icon.find('[mod-role=looper-state]');
        const dubCount = event.icon.find('[mod-role=dub-count]');

        switch (symbol) {
            case 'looper_state':
                const stateIndex = Math.trunc(value);
                const stateName = STATE_NAMES[stateIndex] || "UNKNOWN";
                
                // Update text
                stateText.text(stateName);
                
                // Update CSS class for coloring
                STATE_CLASSES.forEach(function(cls) {
                    display.removeClass(cls);
                });
                if (stateIndex >= 0 && stateIndex < STATE_CLASSES.length) {
                    display.addClass(STATE_CLASSES[stateIndex]);
                }
                break;

            case 'dub_count':
                const count = Math.trunc(value);
                dubCount.text("Layers: " + count);
                break;
        }
    }

    if (event.type == 'start') {
        const ports = event.ports;
        for (let p in ports) {
            handle_event(ports[p].symbol, ports[p].value);
        }
    } else if (event.type == 'change') {
        handle_event(event.symbol, event.value);
    }
}
