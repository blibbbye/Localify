const {contextBridge,ipcRenderer} = require("electron");

contextBridge.exposeInMainWorld("LocalifyDesktop",{
  version:"1.0.0",
  getSaveRoot:()=>ipcRenderer.invoke("localify:save-root"),
  openFiles:()=>ipcRenderer.invoke("localify:open-files"),
  launchDiscordPlayer:()=>ipcRenderer.invoke("localify:launch-discord")
});
