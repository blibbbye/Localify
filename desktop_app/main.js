const {app,BrowserWindow,Menu,shell,ipcMain,dialog} = require("electron");
const path = require("path");
const fs = require("fs");
const {spawn} = require("child_process");

const APP_NAME = "Localify Desktop";
const SAVE_ROOT = path.join(app.getPath("appData"), APP_NAME, "files");
app.setName(APP_NAME);
app.setPath("userData", SAVE_ROOT);

let win = null;

function ensureSaveFolders(){
  for(const dir of [
    SAVE_ROOT,
    path.join(SAVE_ROOT,"audio"),
    path.join(SAVE_ROOT,"covers"),
    path.join(SAVE_ROOT,"presets")
  ]){
    fs.mkdirSync(dir,{recursive:true});
  }
}

function resourcePath(...parts){
  return path.join(process.resourcesPath,"localify",...parts);
}

function openSaveFolder(){
  ensureSaveFolders();
  shell.openPath(SAVE_ROOT);
}

function discordPlayerPath(){
  if(process.defaultApp){
    return path.resolve(__dirname,"..","Localify Discord Player.exe");
  }
  return resourcePath("Localify Discord Player.exe");
}

function launchDiscordPlayer(){
  const exe=discordPlayerPath();
  if(!fs.existsSync(exe)){
    dialog.showErrorBox("Localify Desktop","The bundled Localify Discord Player was not found.");
    return false;
  }
  try{
    spawn(exe,[],{detached:true,stdio:"ignore",windowsHide:true}).unref();
    return true;
  }catch(err){
    dialog.showErrorBox("Localify Desktop","Could not start the Discord player.\\n\\n"+String(err?.message||err));
    return false;
  }
}

function buildMenu(){
  const template=[
    {
      label:"File",
      submenu:[
        {label:"Open Localify Files",click:openSaveFolder},
        {type:"separator"},
        {label:"Launch Localify Discord Player",click:launchDiscordPlayer},
        {type:"separator"},
        {role:"quit"}
      ]
    },
    {
      label:"View",
      submenu:[
        {role:"reload"},
        {role:"togglefullscreen"},
        {type:"separator"},
        {role:"toggleDevTools"}
      ]
    },
    {
      label:"Help",
      submenu:[
        {label:"Open Localify Files Folder",click:openSaveFolder}
      ]
    }
  ];
  Menu.setApplicationMenu(Menu.buildFromTemplate(template));
}

function createWindow(){
  win = new BrowserWindow({
    width:1440,
    height:900,
    minWidth:900,
    minHeight:620,
    title:APP_NAME,
    backgroundColor:"#090b0e",
    show:false,
    autoHideMenuBar:false,
    webPreferences:{
      preload:path.join(__dirname,"preload.js"),
      contextIsolation:true,
      nodeIntegration:false,
      sandbox:true
    }
  });

  win.once("ready-to-show",()=>win.show());

  win.webContents.setWindowOpenHandler(({url})=>{
    if(/^https?:\\/\\//i.test(url)){
      shell.openExternal(url);
    }
    return {action:"deny"};
  });

  win.webContents.on("will-navigate",(event,url)=>{
    if(!url.startsWith("file://")){
      event.preventDefault();
      if(/^https?:\\/\\//i.test(url))shell.openExternal(url);
    }
  });

  win.webContents.on("render-process-gone",(_event,details)=>{
    dialog.showMessageBox({
      type:"error",
      title:APP_NAME,
      message:"Localify Desktop closed the player renderer unexpectedly.",
      detail:String(details?.reason||"Unknown renderer error")
    });
  });

  win.loadFile(resourcePath("index.html")).catch(err=>{
    dialog.showErrorBox(APP_NAME,"Could not load Localify.\\n\\n"+String(err?.message||err));
  });

  win.on("closed",()=>{win=null;});
}

ipcMain.handle("localify:save-root",()=>SAVE_ROOT);
ipcMain.handle("localify:open-files",()=>{openSaveFolder();return SAVE_ROOT;});
ipcMain.handle("localify:launch-discord",()=>launchDiscordPlayer());

app.whenReady().then(()=>{
  ensureSaveFolders();
  buildMenu();
  createWindow();

  app.on("activate",()=>{
    if(BrowserWindow.getAllWindows().length===0)createWindow();
  });
});

app.on("window-all-closed",()=>{
  if(process.platform!=="darwin")app.quit();
});
