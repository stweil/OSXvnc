/* vncController */

/*
 *  OSXvnc Copyright (C) 2001 Dan McGuirk <mcguirk@incompleteness.net>.
 *  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

import com.apple.cocoa.foundation.*;
import com.apple.cocoa.application.*;
import java.util.Vector;
import java.io.File;

public class vncController {
    NSWindow window;
    boolean windowIsVisible = true;
    NSPopUpButton displayNumberField;
    NSTextField portField;
    NSTextField statusMessageField;
    NSButton startServerButton;
    NSButton stopServerButton;
    NSButton dontDisconnectCheckbox;
    NSMenuItem hideOrShowWindowMenuItem;
    NSMenuItem startServerMenuItem;
    NSMenuItem stopServerMenuItem;
    NSTextField passwordField;
    NSTextField displayNameField;
    NSMatrix sharingMatrix;
    NSButton allowDimmingCheckbox;
    NSButton allowSleepCheckbox;
    
    int displayNumber = 0;
    int port = 5900;
    
    boolean alwaysShared = false;
    boolean neverShared = false;
    
    processController controller;
    
    private static String passwordFile = "/tmp/osxvnc-passwd";

    public void loadUserDefaults() {
        displayNumber.selectItemAtIndex(NSUserDefaults.standardUserDefaults().intForKey("displayNumber"));
    }

    public void saveUserDefaults() {
        NSUserDefaults.standardUserDefaults().setIntForKey(displayNumberField.indexOfSelectedItem(),"displayNumber");
        //NSUserDefaults.standardUserDefaults().setObjectForKey(displayNumber,"displayNumber");
        NSUserDefaults.standardUserDefaults().synchronize();
    }
    
    public void changeDisplayNumber(NSPopUpButton sender) {
        displayNumber = sender.indexOfSelectedItem();
        if (displayNumber == 21) {
            // Selected the '--' element.  Don't change the port.
        } else {
            port = displayNumber + 5900;
            portField.setIntValue(port);
        }
    }

    public void changePort(NSTextField sender) {
        port = sender.intValue();
        displayNumber = port - 5900;
        if (displayNumber < 0 || displayNumber > 20) {
            displayNumber = 21;
        }
        displayNumberField.selectItemAtIndex(displayNumber);
    }

    public void changeSharing(NSMatrix sender) {
        int selected = sender.selectedRow();
        if (selected == 0) {
            // Always shared.
            alwaysShared = true;
            neverShared = false;
            dontDisconnectCheckbox.setEnabled(false);
        } else if (selected == 1) {
            // Never shared.
            alwaysShared = false;
            neverShared = true;
            dontDisconnectCheckbox.setEnabled(true);
        } else if (selected == 2) {
            // Not always or never shared.
            alwaysShared = false;
            neverShared = false;
            dontDisconnectCheckbox.setEnabled(true);
        }
    }

    private boolean writePasswordToFile(String password) {
        String[] arguments = new String[3];
        
        arguments[0] = NSBundle.mainBundle().bundlePath() + "/Contents/MacOS/storepasswd";
        arguments[1] = password;
        arguments[2] = passwordFile;
        
        Process process;
        try {
            process = Runtime.getRuntime().exec(arguments);
        } catch(java.io.IOException e) {
            return false;
        }

        try {
            if (process.waitFor() != 0)
                return false;
        } catch(InterruptedException e) {
            return false;
        }
        
        return true;
    }
    
    private String[] formCommandLine() {
        Vector argv = new Vector();
        
        argv.addElement(NSBundle.mainBundle().bundlePath() + "/Contents/MacOS/OSXvnc-server");
        argv.addElement("-rfbport");
        argv.addElement(new Integer(portField.stringValue()).toString());
        argv.addElement("-desktop");
        argv.addElement(displayNameField.stringValue());
        if (alwaysShared) {
            argv.addElement("-alwaysshared");
        }
        if (neverShared) {
            argv.addElement("-nevershared");
        }
        boolean dontDisconnect = (dontDisconnectCheckbox.state() == NSCell.OnState);
        if (dontDisconnect && !alwaysShared) {
            argv.addElement("-dontdisconnect");
        }
        boolean allowDimming = (allowDimmingCheckbox.state() == NSCell.OnState);
        if (!allowDimming) {
            argv.addElement("-nodimming");
        }
        boolean allowSleep = (allowSleepCheckbox.state() == NSCell.OnState);
        if (allowSleep) {
            argv.addElement("-allowsleep");
        }
        String password = passwordField.stringValue();
        if (!password.equals("")) {
            if (!writePasswordToFile(password)) {
                statusMessageField.setStringValue("Unable to start server: couldn't store password.");
                return null;
            }
            argv.addElement("-rfbauth");
            argv.addElement(passwordFile);
        }
        
        String[] arguments = new String[argv.size()];
        for (int i = 0; i < argv.size(); i++) {
            arguments[i] = (String)argv.elementAt(i);
        }
        return arguments;
    }
    
    public void disableEverything() {
        displayNumberField.setEnabled(false);
        portField.setEnabled(false);
        passwordField.setEnabled(false);
        displayNameField.setEnabled(false);
        sharingMatrix.setEnabled(false);
        dontDisconnectCheckbox.setEnabled(false);
        allowDimmingCheckbox.setEnabled(false);
        allowSleepCheckbox.setEnabled(false);
    }
    
    public void enableEverything() {
        displayNumberField.setEnabled(true);
        portField.setEnabled(true);
        passwordField.setEnabled(true);
        displayNameField.setEnabled(true);
        sharingMatrix.setEnabled(true);
        if (!alwaysShared) {
            dontDisconnectCheckbox.setEnabled(true);
        }
        allowDimmingCheckbox.setEnabled(true);
        allowSleepCheckbox.setEnabled(true);
    }
    
    public void startServer(Object sender) {
        // They may have the crossbar in the 'port' field but not have
        // hit 'return'.  So make sure we do an update on the port.
        changePort(portField);
        
        String[] command_line = formCommandLine();
        if (command_line == null)
            return;
        controller = new processController(this);
        if (controller.startProcess(command_line)) {
            statusMessageField.setStringValue("The server is running.");
            startServerButton.setEnabled(false);
            stopServerButton.setEnabled(true);

            // Make it clear that changing options while the server is running
            // is not going to do anything.
            disableEverything();        
        }
    }

    public void stopServer(Object sender) {
        if (controller != null) {
            controller.stopProcess();
            controller = null;
        }
        
        statusMessageField.setStringValue("The server is not running.");
        startServerButton.setEnabled(true);
        startServerMenuItem.setEnabled(true);
        stopServerButton.setEnabled(false);
        stopServerMenuItem.setEnabled(false);
        
        new File(passwordFile).delete();
        
        enableEverything();
    }
    
    public void processStopped(String reason) {
        if (reason != null) {
            statusMessageField.setStringValue("The server has stopped running: " + reason);
        } else {
            statusMessageField.setStringValue("The server has stopped running.");
        }
        controller = null;
        startServerButton.setEnabled(true);
        stopServerButton.setEnabled(false);
        enableEverything();
    }
    
    public void applicationWillTerminate(NSNotification notification) {
        stopServer(null);
    }
    
    private void showWindow() {
        window.makeKeyAndOrderFront(null);
        windowIsVisible = true;
        hideOrShowWindowMenuItem.setTitle("Hide Window");
    }

    private void hideWindow() {
        window.orderOut(null);
        windowIsVisible = false;
        hideOrShowWindowMenuItem.setTitle("Show Window");
    }
    
    public void hideOrShowWindow(NSMenuItem sender) {
        // Make the window visible.
        if (!windowIsVisible) {
            showWindow();
        } else {
            hideWindow();
        }
    }
    
    public boolean windowShouldClose(NSWindow sender) {
        // Make the window not visible, but don't actually close it.
        hideWindow();
        return false;
    }
    
    public boolean validateMenuItem(NSMenuItem menuItem) {
        // Disable the 'start server' menu item when the server is already started.
        // Disable the 'stop server' menu item when the server is not running.
        if ((menuItem == startServerMenuItem) && (controller != null)) {
            return false;
        } else if ((menuItem == stopServerMenuItem) && (controller == null)) {
            return false;
        } else {
            return true;
        }
    }
}
