## Electron Windows OSK
###### Windows on-screen keyboard manager for Electron apps.

This experimental package allows you to show, close, resize and position the built in on-screen Windows keyboard. 
Take a look at `export.js` to check out the API.

Short example: 

```javascript

import WindowsOSK from 'electron-windows-osk';
const OSK = new WindowsOSK(browserWindowInstance);

// Open and position OSK on click in inputs.
document.addEventListener('click', (event) => {
    if (event.target.tagName === 'INPUT' && event.target.tagName === 'TEXTAREA') {
        const viewportOffset = event.target.getBoundingClientRect();
        const inBrowserYOffset =
            (
                viewportOffset.top - // Element top offset.
                window.scrollY       // If the page is scrolled we need to substract the scroll y.
            );

        OSK.showFromEvent(inBrowserYOffset, event.target.clientHeight, 5);
        // meteor-desktop: Desktop.send('virtualKeyboard', 'show', inBrowserYOffset, event.target.clientHeight, 5);
    }
});

// Close the OSK on any other click.
document.addEventListener('click', (event) => {
    if (event.target.tagName !== 'INPUT' && event.target.tagName !== 'TEXTAREA') {
        OSK.close();
        // meteor-desktop: Desktop.send('virtualKeyboard', 'close');
    }
});
```

Example of `meteor-desktop` module:

```javascript
import WindowsOSK from 'electron-windows-osk';

export default class VirtualKeyboardModule {
    constructor({ log, eventsBus, Module }) {
        this.log = log;
        let virtualKeyboardModule = new Module('virtualKeyboard');

        eventsBus.on('windowCreated', (window) => {
            this.window = window;
            console.log(WindowsOSK);
            this.windowsOSK = new WindowsOSK(window);
        });

        virtualKeyboardModule.on('show', this.showKeyboard.bind(this));
        virtualKeyboardModule.on('close', this.closeKeyboard.bind(this));

        this.windowsOSK = null;
    }

    showKeyboard(event, inBrowserYOffset, height, padding) {
        this.windowsOSK.showFromEvent(inBrowserYOffset, height, padding);
    }

    closeKeyboard() {
        this.windowsOSK.close();
    }
};

```