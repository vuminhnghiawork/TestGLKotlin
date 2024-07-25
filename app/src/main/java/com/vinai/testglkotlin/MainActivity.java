package com.vinai.testglkotlin;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.ImageView;
import android.widget.TextView;
import android.Manifest;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.os.Environment;
import java.io.File;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("test_app_native_renderer"); // Tên module bạn đã khai báo trong Android.mk
        System.loadLibrary("test_app_native"); // Tên module bạn đã khai báo trong Android.mk
    }

    private static final int PERMISSION_REQUEST_CODE = 100;
    public native String stringFromJNI();
    public native void initSurface(Surface surface);
    public native void deinitSurface();
    public native void surfaceResize();
    public native int loadTextureFromFile(String picturesDir);

    private static final String TAG = "MainActivity";
    private ImageView myImageView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        myImageView = findViewById(R.id.myImageView);

        // Kiểm tra và yêu cầu quyền truy cập vào bộ nhớ ngoài nếu cần
        if (ContextCompat.checkSelfPermission(this,
                Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {

            // Yêu cầu quyền
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    PERMISSION_REQUEST_CODE);
        } else {
            // Quyền đã được cấp, bạn có thể truy cập vào bộ nhớ ngoài
            accessExternalStorage();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                // Quyền đã được cấp
                accessExternalStorage();
            } else {
                // Quyền bị từ chối
                Log.e("MainActivity", "Permission denied to write to external storage");
            }
        }

        // Lấy đường dẫn đến thư mục ảnh công cộng
        File picturesDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        picturesDir = new File(picturesDir, "1045-2.jpg");
        if (picturesDir != null) {
            Log.d("MainActivity", "Pictures Directory: " + picturesDir.getAbsolutePath());
            int textureId = loadTextureFromFile(picturesDir.getAbsolutePath());
        }

        // Sử dụng phương thức JNI
        TextView textView = findViewById(R.id.textView);
        textView.setText(stringFromJNI());

        SurfaceView surfaceView = findViewById(R.id.surfaceView);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
                initSurface(surfaceHolder.getSurface());
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {
                surfaceResize();
            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
                deinitSurface();
            }
        });
    };

    private void accessExternalStorage() {
        // Thực hiện các thao tác truy cập bộ nhớ ngoài
        File picturesDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        picturesDir = new File(picturesDir, "1045-2.jpg");
        if (picturesDir != null) {
            Log.d("MainActivity", "Pictures Directory: " + picturesDir.getAbsolutePath());
            int textureId = loadTextureFromFile(picturesDir.getAbsolutePath());
        }
    };
    private void loadImageFromUri() {
        // Đường dẫn tệp hình ảnh trong bộ nhớ ngoài
        File picturesDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        File imageFile = new File(picturesDir, "1045-2.jpg");

        if (imageFile.exists()) {
            Uri imageUri = Uri.fromFile(imageFile);
            myImageView.setImageURI(imageUri);
        } else {
            Log.e(TAG, "File does not exist: " + imageFile.getAbsolutePath());
        }
    }
}