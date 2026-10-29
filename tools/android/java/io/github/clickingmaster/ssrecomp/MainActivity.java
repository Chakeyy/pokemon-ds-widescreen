package io.github.clickingmaster.ssrecomp;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.util.Log;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileWriter;

public class MainActivity extends SDLActivity {
    private static final String TAG = "PokemonDS";
    public static final int REQUEST_CODE_FOLDER = 9901;
    public static final int REQUEST_CODE_FILE = 9902;

    public static volatile String sLatestPickedFolder = null;
    public static volatile String sLatestPickedRom = null;

    public void openFolderPicker() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
                    intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION 
                                  | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                                  | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                    startActivityForResult(intent, REQUEST_CODE_FOLDER);
                } catch (Exception e) {
                    Log.e(TAG, "Error launching ACTION_OPEN_DOCUMENT_TREE: " + e.getMessage());
                }
            }
        });
    }

    public void openFilePicker() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
                    intent.addCategory(Intent.CATEGORY_OPENABLE);
                    intent.setType("*/*");
                    intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                                  | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
                    startActivityForResult(intent, REQUEST_CODE_FILE);
                } catch (Exception e) {
                    Log.e(TAG, "Error launching ACTION_OPEN_DOCUMENT: " + e.getMessage());
                }
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != Activity.RESULT_OK || data == null) return;

        Uri uri = data.getData();
        if (uri == null) return;

        try {
            getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception ignored) {}

        String fullPath = null;
        try {
            if (requestCode == REQUEST_CODE_FOLDER) {
                String treeDocId = DocumentsContract.getTreeDocumentId(uri);
                fullPath = resolveDocId(treeDocId);
            } else if (requestCode == REQUEST_CODE_FILE) {
                String docId = DocumentsContract.getDocumentId(uri);
                fullPath = resolveDocId(docId);
            }
        } catch (Exception e) {
            Log.e(TAG, "Error resolving Document ID: " + e.getMessage());
        }

        if (fullPath == null || fullPath.isEmpty()) {
            fullPath = uri.getPath();
            if (fullPath != null && fullPath.contains(":")) {
                fullPath = resolveDocId(fullPath.substring(fullPath.indexOf(':') + 1));
            }
        }

        if (fullPath != null && !fullPath.isEmpty()) {
            Log.i(TAG, "Successfully resolved DocumentsUI path: " + fullPath);
            File stateDir = getExternalFilesDir(null);
            if (stateDir == null) stateDir = getFilesDir();

            if (requestCode == REQUEST_CODE_FOLDER) {
                sLatestPickedFolder = fullPath;
                try {
                    File f = new File(stateDir, "rom_dir.txt");
                    FileWriter fw = new FileWriter(f);
                    fw.write(fullPath + "\n");
                    fw.close();
                } catch (Exception e) {
                    Log.e(TAG, "Failed writing rom_dir.txt: " + e.getMessage());
                }
            } else if (requestCode == REQUEST_CODE_FILE) {
                sLatestPickedRom = fullPath;
                try {
                    File f = new File(stateDir, "last_rom.txt");
                    FileWriter fw = new FileWriter(f);
                    fw.write(fullPath + "\n");
                    fw.close();
                } catch (Exception e) {
                    Log.e(TAG, "Failed writing last_rom.txt: " + e.getMessage());
                }
            }
        }
    }

    private String resolveDocId(String docId) {
        if (docId == null) return null;
        if (docId.startsWith("raw:")) {
            return docId.substring(4);
        }
        String[] split = docId.split(":", 2);
        String type = split[0];
        String sub = split.length > 1 ? split[1] : "";
        if ("primary".equalsIgnoreCase(type)) {
            String base = Environment.getExternalStorageDirectory().getAbsolutePath();
            return sub.isEmpty() ? base : (base + "/" + sub);
        } else {
            return sub.isEmpty() ? ("/storage/" + type) : ("/storage/" + type + "/" + sub);
        }
    }
}
