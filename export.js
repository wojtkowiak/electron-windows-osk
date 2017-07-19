var screen = require('electron').screen;
var native = require('./build/Release/addon');

/**
 * @param {BrowserWindow} browserWindow                  - reference to the browser window the OSK
 *                                                         is shown in reference to
 * @param {boolean}       menuPresent                    - whether the browser window has a menu
 * @param {number}        keyboardHeightScreenPercentage - the height of the OSK in screen height
 *                                                         percentage
 * @constructor
 */
function OSK(browserWindow, menuPresent, keyboardHeightScreenPercentage) {
    this.browserWindow = browserWindow;
    this.menuPresent = menuPresent;
    this.keyboardHeightScreenPercentage = keyboardHeightScreenPercentage || 30;
}

/**
 * Makes the OSK to appear.
 *
 * @param {number} x      - position at screen x
 * @param {number} y      - position at screen y
 * @param {number} width  - width of the OSK
 * @param {number} height - height of the OSK
 * @returns {boolean}
 */
OSK.prototype.show = function(x, y, width, height) {
    // Close the OSK if it is open.
    if (this.close()) {
        // Set it's position and size.
        if (native.OSKSetPosition(x,y,width,height)) {
            // Lauch it.
            return native.OSKShow();
        }
    }
    return false;
};

/**
 * Makes the OSK to close.
 * @returns {boolean}
 */
OSK.prototype.close = function() {
    if (this.isVisible()) {
        return native.OSKClose();
    }
    return true;
};

/**
 * Checks if the OSK is visible.
 * @returns {boolean}
 */
OSK.prototype.isVisible = function() {
    return native.OSKIsVisible();
};

/**
 * Gets some system specific window metrics.
 * @returns {{topBarThickness: number, borderThickness: number, menuHeight: number}}
 */
OSK.prototype.getSystem = function() {
    return {
        topBarThickness: native.GetSystemTopBarThickness(),
        borderThickness: native.GetSystemBorderThickness(),
        menuHeight: native.GetSystemMenuHeight()
    };
};

/**
 * Shows the OSK in regard to a html event. Tries to position the OSK according to event.target
 * element.
 * @param {number} inBrowserYOffset - browser event
 * @param {number} yOffset          - additional y offset for the OSK
 * @returns {boolean}
 */
OSK.prototype.showFromEvent = function(inBrowserYOffset, yOffset) {
    if (yOffset === undefined) {
        yOffset = 0;
    }
    // We need to know where the browser window is.
    var windowPosition = this.browserWindow.getPosition();

    // Some system window metrics will be useful.
    var system = this.getSystem();
    
    var menuHeight = 0;
    // In development mode Electron always has a menu. Otherwise we can not detect it so we rely on
    // menuPresent flag to take the menu height into account.
    if (process.env.ELECTRON_ENV === 'development' || this.menuPresent) {
        menuHeight = system.menuHeight;
    }

    var topBarHeight = system.topBarThickness;
    var borderCount = 2; // Normally we have two borders - one for the browser window itself and OSK.

    if (this.browserWindow.isFullScreen()) {
        topBarHeight = 0;
        // In full screen there is no top bar so we only have one border.
        borderCount = 1;
    }

    var top =
        inBrowserYOffset +   // the y position in the view part of the browser
        windowPosition[1] +  // the screen y of the browser window position
        menuHeight +         // height of the menu if present
        topBarHeight +       // height of the window top bar
        (system.borderThickness * borderCount) + // total height of the window borders
        yOffset;             // additional screen y offset

    // Now we calculate the OSK height as the percentage of the screen height.
    var display = screen.getPrimaryDisplay();
    var oskHeight = Math.floor(
        (display.workAreaSize.height * this.keyboardHeightScreenPercentage) / 100
    );

    // OSK will be as wide as possible by default.
    var oskWidth = display.workAreaSize.width;

    return this.show(0,top, oskWidth, oskHeight);
};

module.exports = OSK;
