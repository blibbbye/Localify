const fs = require("fs");
const path = require("path");

const root = process.cwd();
const android = path.join(root, "android");
if (!fs.existsSync(android)) throw new Error("Android project was not generated.");

const appSrc = path.join(android, "app", "src", "main");
const manifestPath = path.join(appSrc, "AndroidManifest.xml");
const manifest = fs.readFileSync(manifestPath, "utf8");

function walk(dir) {
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walk(p));
    else out.push(p);
  }
  return out;
}

const mainActivity = walk(appSrc).find(p => /MainActivity\.(java|kt)$/.test(p));
if (!mainActivity) throw new Error("MainActivity.java/kt not found.");

const activitySource = fs.readFileSync(mainActivity, "utf8");
const pkg = (activitySource.match(/^\s*package\s+([A-Za-z0-9_.]+)\s*;/m) || [])[1];
if (!pkg) throw new Error("Could not determine Android package name.");

const javaDir = path.dirname(mainActivity);
const servicePath = path.join(javaDir, "LocalifyPlaybackService.java");

const service = `package ${pkg};

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;

public final class LocalifyPlaybackService extends Service {
    private static final int NOTIFICATION_ID = 7001;
    private static final String CHANNEL_ID = "localify_playback";

    @Override
    public void onCreate() {
        super.onCreate();
        createChannel();
        startForeground(NOTIFICATION_ID, buildNotification());
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        startForeground(NOTIFICATION_ID, buildNotification());
        return START_STICKY;
    }

    private Notification buildNotification() {
        Intent launch = getPackageManager().getLaunchIntentForPackage(getPackageName());
        PendingIntent pending = null;
        if (launch != null) {
            pending = PendingIntent.getActivity(this, 0, launch,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        }

        Notification.Builder builder = Build.VERSION.SDK_INT >= 26
                ? new Notification.Builder(this, CHANNEL_ID)
                : new Notification.Builder(this);

        builder.setContentTitle("Localify")
                .setContentText("Playing your local music")
                .setSmallIcon(android.R.drawable.ic_media_play)
                .setOngoing(true)
                .setCategory(Notification.CATEGORY_TRANSPORT);

        if (pending != null) builder.setContentIntent(pending);
        return builder.build();
    }

    private void createChannel() {
        if (Build.VERSION.SDK_INT < 26) return;
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID, "Localify playback", NotificationManager.IMPORTANCE_LOW);
        channel.setDescription("Keeps Localify playback active in the background.");
        NotificationManager manager = getSystemService(NotificationManager.class);
        if (manager != null) manager.createNotificationChannel(channel);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
`;

fs.writeFileSync(servicePath, service, "utf8");

let updatedManifest = manifest;
const permissionLines = [
  '    <uses-permission android:name="android.permission.FOREGROUND_SERVICE" />',
  '    <uses-permission android:name="android.permission.FOREGROUND_SERVICE_MEDIA_PLAYBACK" />',
  '    <uses-permission android:name="android.permission.POST_NOTIFICATIONS" />'
].join("\n");
if (!updatedManifest.includes("android.permission.FOREGROUND_SERVICE")) {
  updatedManifest = updatedManifest.replace(/(<manifest[^>]*>)/, "$1\n" + permissionLines);
}
if (!updatedManifest.includes("LocalifyPlaybackService")) {
  const serviceLine = '        <service android:name=".LocalifyPlaybackService" android:exported="false" android:foregroundServiceType="mediaPlayback" />\n';
  updatedManifest = updatedManifest.replace(/\s*<\/application>/, "\n" + serviceLine + "    </application>");
}
fs.writeFileSync(manifestPath, updatedManifest, "utf8");

if (!/\.java$/.test(mainActivity)) {
  throw new Error("Expected Capacitor MainActivity.java, but a Kotlin activity was generated.");
}

let activity = activitySource;
if (!activity.includes("android.webkit.JavascriptInterface")) {
  activity = activity.replace(/(package [^;]+;\n)/, "$1\nimport android.content.Intent;\nimport android.os.Build;\nimport android.webkit.JavascriptInterface;\n");
}
if (!activity.includes("attachLocalifyPlaybackBridge")) {
  const classOpen = activity.indexOf("{", activity.indexOf("class MainActivity"));
  const bridgeMethod = `
    private void attachLocalifyPlaybackBridge() {
        if (getBridge() == null || getBridge().getWebView() == null) return;
        getBridge().getWebView().addJavascriptInterface(new Object() {
            @JavascriptInterface
            public void startPlaybackService() {
                Intent intent = new Intent(MainActivity.this, LocalifyPlaybackService.class);
                if (Build.VERSION.SDK_INT >= 26) {
                    startForegroundService(intent);
                } else {
                    startService(intent);
                }
            }

            @JavascriptInterface
            public void stopPlaybackService() {
                stopService(new Intent(MainActivity.this, LocalifyPlaybackService.class));
            }
        }, "LocalifyNative");
    }

    @Override
    protected void onCreate(android.os.Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        attachLocalifyPlaybackBridge();
        if (Build.VERSION.SDK_INT >= 33 &&
                checkSelfPermission("android.permission.POST_NOTIFICATIONS") != android.content.pm.PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{"android.permission.POST_NOTIFICATIONS"}, 701);
        }
    }

`;
  activity = activity.slice(0, classOpen + 1) + bridgeMethod + activity.slice(classOpen + 1);
}
fs.writeFileSync(mainActivity, activity, "utf8");

console.log("Configured foreground playback service and LocalifyNative bridge.");
