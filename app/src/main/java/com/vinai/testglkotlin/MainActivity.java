package com.vinai.testglkotlin;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.Manifest;
import android.content.pm.PackageManager;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import android.os.Environment;
import android.widget.Toast;

import java.io.File;

public class MainActivity extends AppCompatActivity {
    static {
        System.loadLibrary("test_app_native_renderer"); // Tên module bạn đã khai báo trong Android.mk
    }

    private static final int PERMISSION_REQUEST_CODE = 100;
    public native void initSurface(Surface surface, String picturesDir);
    public native void deinitSurface();
    public native void surfaceResize();


    private static final String TAG = "MainActivity";
    private ImageView myImageView;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

    SurfaceView surfaceView = findViewById(R.id.surfaceView);
        surfaceView.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
                // Lấy đường dẫn đến thư mục ảnh công cộng
                File picturesDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
                picturesDir = new File(picturesDir, "1045-2.jpg");
                if (picturesDir != null) {
                    Log.d("MainActivity", "Pictures Directory: " + picturesDir.getAbsolutePath());
                    SurfaceView surfaceView = findViewById(R.id.surfaceView);
                    Surface surface = surfaceView.getHolder().getSurface();
                }
                Toast.makeText(MainActivity.this, "dcmm!", Toast.LENGTH_SHORT).show();

                initSurface(surfaceHolder.getSurface(), picturesDir.getAbsolutePath());
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

        myImageView = findViewById(R.id.myImageView);

        Button myButton = findViewById(R.id.myButton);
        myButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // Xử lý khi button được nhấp
                int color = ContextCompat.getColor(MainActivity.this, R.color.button_pressed_color);
                getWindow().getDecorView().setBackgroundColor(color);

                Toast.makeText(MainActivity.this, "dcmm!", Toast.LENGTH_SHORT).show();
            }
        });

        // Yêu cầu quyền
        ActivityCompat.requestPermissions(this,
                new String[]{Manifest.permission.READ_EXTERNAL_STORAGE, Manifest.permission.READ_MEDIA_IMAGES},
                PERMISSION_REQUEST_CODE);
        // Quyền đã được cấp, bạn có thể truy cập vào bộ nhớ ngoài
        accessExternalStorage();
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
    };

    private void accessExternalStorage() {
        // Thực hiện các thao tác truy cập bộ nhớ ngoài
        File picturesDir = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
        picturesDir = new File(picturesDir, "1045-2.jpg");
        if (picturesDir != null) {
            Log.d("MainActivity", "Pictures Directory: " + picturesDir.getAbsolutePath());
//            int textureId = loadTextureFromFile(picturesDir.getAbsolutePath());
//            loadImageFromUri();
        }
    }
    private void loadImageFromUri() {
        myImageView.setVisibility(View.GONE);
    }
}