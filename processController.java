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

public class processController extends Thread {
    vncController listener;
    Process process;
    boolean listenerRequestedStop = false;
    
    public processController(vncController listener) {
        this.listener = listener;
        this.process = null;
    }
    
    public boolean startProcess(String[] command_line) {
        try { 
            process = Runtime.getRuntime().exec(command_line);
        } catch(Exception e) {
            listener.processStopped("Failed to start process: " + e);
            process = null;
            return false;
        }
        
        start();
        return true;
    }
    
    public void stopProcess() {
        listenerRequestedStop = true;
        if (process != null) {
            process.destroy();
        }
        // Should cause process.waitFor() in our thread to return,
        // and then we'll tell the listener that the process went away.
        
        // We want to join the thread at this point so we
        // don't return until the process is actually dead.
        try {
            join();
        } catch(InterruptedException e) {}    
    }
    
    public void run() {
        boolean done = false;
        int resultValue = -1;
        
        while (!done) {
            try {
                resultValue = process.waitFor();
                done = true;
            } catch(InterruptedException e) {}
        }
        
        // If we were stopped by the client, don't bother telling them the process
        // actually stopped.  This is designed to notify them of unexpected stops.
        if (!listenerRequestedStop) {
            listener.processStopped("Process exited with status " + resultValue + ".");
        }
        process = null;
    }
}
