package com.winland.server;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.Button;
import android.widget.Filter;
import android.widget.Filterable;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.ProgressBar;
import android.widget.SearchView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.localbroadcastmanager.content.LocalBroadcastManager;
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout;

import com.google.android.material.chip.Chip;
import com.google.android.material.chip.ChipGroup;
import com.google.android.material.floatingactionbutton.FloatingActionButton;
import com.google.android.material.tabs.TabLayout;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * نشاط مدير الحزم - واجهة المستخدم الرسومية لتثبيت وإدارة حزم Linux
 */
public class PackageManagerActivity extends Activity {
    
    private static final String TAG = "PackageManager";
    
    // الفئات
    private static final String[] CATEGORIES = {
        "all", "internet", "office", "development", "media", 
        "graphics", "games", "tools", "communication", "desktop"
    };
    
    private ListView packageListView;
    private PackageAdapter packageAdapter;
    private SwipeRefreshLayout swipeRefresh;
    private TabLayout tabLayout;
    private ChipGroup categoryChips;
    private SearchView searchView;
    private ProgressBar progressBar;
    private TextView statusText;
    private FloatingActionButton fabUpdate;
    
    private Handler mainHandler;
    private ProgressDialog progressDialog;
    
    private List<PackageInfo> allPackages = new ArrayList<>();
    private List<PackageInfo> installedPackages = new ArrayList<>();
    private List<PackageInfo> availablePackages = new ArrayList<>();
    private List<PackageInfo> updatePackages = new ArrayList<>();
    
    private String currentCategory = "all";
    private int currentTab = 0;
    
    // استقبال الأحداث من الخدمة
    private BroadcastReceiver packageReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action == null) return;
            
            switch (action) {
                case "PACKAGE_LIST_UPDATED":
                    String packagesJson = intent.getStringExtra("packages");
                    parsePackages(packagesJson);
                    break;
                    
                case "PACKAGE_INSTALL_PROGRESS":
                    String packageName = intent.getStringExtra("package");
                    int progress = intent.getIntExtra("progress", 0);
                    updateInstallProgress(packageName, progress);
                    break;
                    
                case "PACKAGE_INSTALL_COMPLETE":
                    String completedPackage = intent.getStringExtra("package");
                    boolean success = intent.getBooleanExtra("success", false);
                    String message = intent.getStringExtra("message");
                    onInstallComplete(completedPackage, success, message);
                    break;
                    
                case "PACKAGE_ERROR":
                    String error = intent.getStringExtra("error");
                    showError(error);
                    break;
            }
        }
    };
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_package_manager);
        
        mainHandler = new Handler(Looper.getMainLooper());
        
        initViews();
        setupListeners();
        registerReceivers();
        
        // تحميل الحزم
        loadPackages();
    }
    
    private void initViews() {
        packageListView = findViewById(R.id.package_list);
        swipeRefresh = findViewById(R.id.swipe_refresh);
        tabLayout = findViewById(R.id.tab_layout);
        categoryChips = findViewById(R.id.category_chips);
        searchView = findViewById(R.id.search_view);
        progressBar = findViewById(R.id.progress_bar);
        statusText = findViewById(R.id.status_text);
        fabUpdate = findViewById(R.id.fab_update);
        
        // إعداد المحول
        packageAdapter = new PackageAdapter();
        packageListView.setAdapter(packageAdapter);
        
        // إعداد التبويبات
        tabLayout.addTab(tabLayout.newTab().setText(R.string.tab_available));
        tabLayout.addTab(tabLayout.newTab().setText(R.string.tab_installed));
        tabLayout.addTab(tabLayout.newTab().setText(R.string.tab_updates));
        
        // إعداد فئات الشرائح
        setupCategoryChips();
    }
    
    private void setupCategoryChips() {
        String[] categoryNames = {
            getString(R.string.cat_all),
            getString(R.string.cat_internet),
            getString(R.string.cat_office),
            getString(R.string.cat_development),
            getString(R.string.cat_media),
            getString(R.string.cat_graphics),
            getString(R.string.cat_games),
            getString(R.string.cat_tools),
            getString(R.string.cat_communication),
            getString(R.string.cat_desktop)
        };
        
        for (int i = 0; i < CATEGORIES.length; i++) {
            Chip chip = new Chip(this);
            chip.setText(categoryNames[i]);
            chip.setCheckable(true);
            chip.setChecked(i == 0);
            chip.setTag(CATEGORIES[i]);
            
            final String category = CATEGORIES[i];
            chip.setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (isChecked) {
                    currentCategory = category;
                    filterPackages();
                    // إلغاء تحديد الشرائح الأخرى
                    for (int j = 0; j < categoryChips.getChildCount(); j++) {
                        View child = categoryChips.getChildAt(j);
                        if (child != buttonView && child instanceof Chip) {
                            ((Chip) child).setChecked(false);
                        }
                    }
                }
            });
            
            categoryChips.addView(chip);
        }
    }
    
    private void setupListeners() {
        // سحب للتحديث
        swipeRefresh.setOnRefreshListener(this::loadPackages);
        
        // تغيير التبويب
        tabLayout.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                currentTab = tab.getPosition();
                filterPackages();
            }
            
            @Override
            public void onTabUnselected(TabLayout.Tab tab) {}
            
            @Override
            public void onTabReselected(TabLayout.Tab tab) {}
        });
        
        // البحث
        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                packageAdapter.getFilter().filter(query);
                return true;
            }
            
            @Override
            public boolean onQueryTextChange(String newText) {
                packageAdapter.getFilter().filter(newText);
                return true;
            }
        });
        
        // زر التحديث
        fabUpdate.setOnClickListener(v -> showUpdateDialog());
    }
    
    private void registerReceivers() {
        IntentFilter filter = new IntentFilter();
        filter.addAction("PACKAGE_LIST_UPDATED");
        filter.addAction("PACKAGE_INSTALL_PROGRESS");
        filter.addAction("PACKAGE_INSTALL_COMPLETE");
        filter.addAction("PACKAGE_ERROR");
        LocalBroadcastManager.getInstance(this).registerReceiver(packageReceiver, filter);
    }
    
    private void loadPackages() {
        swipeRefresh.setRefreshing(true);
        statusText.setText(R.string.loading_packages);
        
        // طلب الحزم من الخدمة الأصلية
        if (WinlandService.isRunning()) {
            WinlandService.getInstance().requestPackageList();
        } else {
            // محاكاة البيانات للاختبار
            simulatePackages();
        }
    }
    
    private void simulatePackages() {
        // محاكاة قائمة الحزم للاختبار
        mainHandler.postDelayed(() -> {
            allPackages.clear();
            
            // إضافة بعض الحزم التجريبية
            allPackages.add(new PackageInfo("Firefox", "Web browser from Mozilla",
                    "Internet", "125.0.1", false, false));
            allPackages.add(new PackageInfo("LibreOffice", "Full office suite",
                    "Office", "24.2.3", false, false));
            allPackages.add(new PackageInfo("VS Code", "Code editor from Microsoft",
                    "Development", "1.89.0", true, false));
            allPackages.add(new PackageInfo("VLC", "Universal media player",
                    "Media", "3.0.20", true, true));
            allPackages.add(new PackageInfo("GIMP", "GNU Image Manipulation Program",
                    "Graphics", "2.10.36", false, false));
            allPackages.add(new PackageInfo("Telegram", "Messaging app",
                    "Communication", "4.16.0", true, false));
            
            filterPackages();
            swipeRefresh.setRefreshing(false);
            statusText.setText(getString(R.string.packages_count, allPackages.size()));
        }, 1000);
    }
    
    private void parsePackages(String json) {
        try {
            JSONArray array = new JSONArray(json);
            allPackages.clear();
            
            for (int i = 0; i < array.length(); i++) {
                JSONObject obj = array.getJSONObject(i);
                PackageInfo pkg = new PackageInfo(
                    obj.getString("name"),
                    obj.getString("description"),
                    obj.getString("category"),
                    obj.getString("version"),
                    obj.getBoolean("installed"),
                    obj.getBoolean("hasUpdate")
                );
                allPackages.add(pkg);
            }
            
            filterPackages();
            
        } catch (JSONException e) {
            showError("Failed to parse packages: " + e.getMessage());
        }
        
        swipeRefresh.setRefreshing(false);
    }
    
    private void filterPackages() {
        List<PackageInfo> filtered = new ArrayList<>();
        
        // تصفية حسب التبويب
        List<PackageInfo> sourceList;
        switch (currentTab) {
            case 0: // متاح
                sourceList = allPackages;
                break;
            case 1: // مثبت
                sourceList = installedPackages;
                for (PackageInfo pkg : allPackages) {
                    if (pkg.installed) {
                        sourceList.add(pkg);
                    }
                }
                break;
            case 2: // تحديثات
                sourceList = updatePackages;
                updatePackages.clear();
                for (PackageInfo pkg : allPackages) {
                    if (pkg.hasUpdate) {
                        updatePackages.add(pkg);
                    }
                }
                break;
            default:
                sourceList = allPackages;
        }
        
        // تصفية حسب الفئة
        for (PackageInfo pkg : sourceList) {
            if (currentCategory.equals("all") || 
                pkg.category.toLowerCase().equals(currentCategory)) {
                filtered.add(pkg);
            }
        }
        
        packageAdapter.setPackages(filtered);
        statusText.setText(getString(R.string.showing_packages, filtered.size()));
    }
    
    private void installPackage(PackageInfo pkg) {
        progressDialog = new ProgressDialog(this);
        progressDialog.setTitle(R.string.installing);
        progressDialog.setMessage(getString(R.string.installing_package, pkg.name));
        progressDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);
        progressDialog.setIndeterminate(false);
        progressDialog.setMax(100);
        progressDialog.show();
        
        if (WinlandService.isRunning()) {
            WinlandService.getInstance().installPackage(pkg.name);
        } else {
            // محاكاة التثبيت
            simulateInstall(pkg);
        }
    }
    
    private void simulateInstall(PackageInfo pkg) {
        new Thread(() -> {
            for (int i = 0; i <= 100; i += 10) {
                final int progress = i;
                mainHandler.post(() -> {
                    if (progressDialog != null) {
                        progressDialog.setProgress(progress);
                    }
                });
                try {
                    Thread.sleep(300);
                } catch (InterruptedException e) {
                    break;
                }
            }
            
            mainHandler.post(() -> {
                if (progressDialog != null) {
                    progressDialog.dismiss();
                }
                pkg.installed = true;
                packageAdapter.notifyDataSetChanged();
                Toast.makeText(this, R.string.install_success, Toast.LENGTH_SHORT).show();
            });
        }).start();
    }
    
    private void removePackage(PackageInfo pkg) {
        new AlertDialog.Builder(this)
            .setTitle(R.string.confirm_remove)
            .setMessage(getString(R.string.confirm_remove_msg, pkg.name))
            .setPositiveButton(R.string.remove, (dialog, which) -> {
                if (WinlandService.isRunning()) {
                    WinlandService.getInstance().removePackage(pkg.name);
                }
                pkg.installed = false;
                packageAdapter.notifyDataSetChanged();
                Toast.makeText(this, R.string.remove_success, Toast.LENGTH_SHORT).show();
            })
            .setNegativeButton(R.string.cancel, null)
            .show();
    }
    
    private void updateInstallProgress(String packageName, int progress) {
        mainHandler.post(() -> {
            if (progressDialog != null && progressDialog.isShowing()) {
                progressDialog.setProgress(progress);
            }
        });
    }
    
    private void onInstallComplete(String packageName, boolean success, String message) {
        mainHandler.post(() -> {
            if (progressDialog != null) {
                progressDialog.dismiss();
            }
            
            if (success) {
                Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
                loadPackages();  // إعادة تحميل القائمة
            } else {
                showError(message);
            }
        });
    }
    
    private void showUpdateDialog() {
        new AlertDialog.Builder(this)
            .setTitle(R.string.update_all)
            .setMessage(R.string.update_all_msg)
            .setPositiveButton(R.string.update, (dialog, which) -> {
                if (WinlandService.isRunning()) {
                    WinlandService.getInstance().upgradeAllPackages();
                }
            })
            .setNegativeButton(R.string.cancel, null)
            .show();
    }
    
    private void showError(String error) {
        mainHandler.post(() -> {
            Toast.makeText(this, error, Toast.LENGTH_LONG).show();
        });
    }
    
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.package_manager_menu, menu);
        return true;
    }
    
    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_refresh) {
            loadPackages();
            return true;
        } else if (id == R.id.action_clean_cache) {
            if (WinlandService.isRunning()) {
                WinlandService.getInstance().cleanPackageCache();
            }
            return true;
        } else if (id == R.id.action_autoremove) {
            if (WinlandService.isRunning()) {
                WinlandService.getInstance().autoremovePackages();
            }
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
    
    @Override
    protected void onDestroy() {
        super.onDestroy();
        LocalBroadcastManager.getInstance(this).unregisterReceiver(packageReceiver);
    }
    
    // ============== فئات المساعدة ==============
    
    private static class PackageInfo {
        String name;
        String description;
        String category;
        String version;
        boolean installed;
        boolean hasUpdate;
        
        PackageInfo(String name, String description, String category,
                    String version, boolean installed, boolean hasUpdate) {
            this.name = name;
            this.description = description;
            this.category = category;
            this.version = version;
            this.installed = installed;
            this.hasUpdate = hasUpdate;
        }
    }
    
    private class PackageAdapter extends BaseAdapter implements Filterable {
        private List<PackageInfo> packages = new ArrayList<>();
        private List<PackageInfo> filteredPackages = new ArrayList<>();
        private PackageFilter filter;
        
        void setPackages(List<PackageInfo> packages) {
            this.packages = packages;
            this.filteredPackages = packages;
            notifyDataSetChanged();
        }
        
        @Override
        public int getCount() {
            return filteredPackages.size();
        }
        
        @Override
        public Object getItem(int position) {
            return filteredPackages.get(position);
        }
        
        @Override
        public long getItemId(int position) {
            return position;
        }
        
        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            ViewHolder holder;
            
            if (convertView == null) {
                convertView = LayoutInflater.from(PackageManagerActivity.this)
                    .inflate(R.layout.item_package, parent, false);
                holder = new ViewHolder();
                holder.icon = convertView.findViewById(R.id.pkg_icon);
                holder.name = convertView.findViewById(R.id.pkg_name);
                holder.description = convertView.findViewById(R.id.pkg_description);
                holder.version = convertView.findViewById(R.id.pkg_version);
                holder.category = convertView.findViewById(R.id.pkg_category);
                holder.status = convertView.findViewById(R.id.pkg_status);
                holder.actionButton = convertView.findViewById(R.id.pkg_action);
                convertView.setTag(holder);
            } else {
                holder = (ViewHolder) convertView.getTag();
            }
            
            PackageInfo pkg = filteredPackages.get(position);
            
            holder.name.setText(pkg.name);
            holder.description.setText(pkg.description);
            holder.version.setText(pkg.version);
            holder.category.setText(pkg.category);
            
            // أيقونة الفئة
            holder.icon.setImageResource(getCategoryIcon(pkg.category));
            
            // حالة الحزمة
            if (pkg.installed) {
                if (pkg.hasUpdate) {
                    holder.status.setText(R.string.status_update_available);
                    holder.status.setTextColor(getColor(R.color.orange));
                    holder.actionButton.setText(R.string.update);
                    holder.actionButton.setOnClickListener(v -> installPackage(pkg));
                } else {
                    holder.status.setText(R.string.status_installed);
                    holder.status.setTextColor(getColor(R.color.green));
                    holder.actionButton.setText(R.string.remove);
                    holder.actionButton.setOnClickListener(v -> removePackage(pkg));
                }
            } else {
                holder.status.setText(R.string.status_not_installed);
                holder.status.setTextColor(getColor(R.color.gray));
                holder.actionButton.setText(R.string.install);
                holder.actionButton.setOnClickListener(v -> installPackage(pkg));
            }
            
            return convertView;
        }
        
        private int getCategoryIcon(String category) {
            switch (category.toLowerCase()) {
                case "internet": return R.drawable.ic_globe;
                case "office": return R.drawable.ic_file_text;
                case "development": return R.drawable.ic_code;
                case "media": return R.drawable.ic_play_circle;
                case "graphics": return R.drawable.ic_image;
                case "games": return R.drawable.ic_gamepad;
                case "tools": return R.drawable.ic_wrench;
                case "communication": return R.drawable.ic_message;
                case "desktop": return R.drawable.ic_monitor;
                default: return R.drawable.ic_package;
            }
        }
        
        @Override
        public Filter getFilter() {
            if (filter == null) {
                filter = new PackageFilter();
            }
            return filter;
        }
        
        private class PackageFilter extends Filter {
            @Override
            protected FilterResults performFiltering(CharSequence constraint) {
                FilterResults results = new FilterResults();
                List<PackageInfo> filtered = new ArrayList<>();
                
                if (constraint == null || constraint.length() == 0) {
                    filtered = packages;
                } else {
                    String query = constraint.toString().toLowerCase();
                    for (PackageInfo pkg : packages) {
                        if (pkg.name.toLowerCase().contains(query) ||
                            pkg.description.toLowerCase().contains(query)) {
                            filtered.add(pkg);
                        }
                    }
                }
                
                results.values = filtered;
                results.count = filtered.size();
                return results;
            }
            
            @Override
            protected void publishResults(CharSequence constraint, FilterResults results) {
                filteredPackages = (List<PackageInfo>) results.values;
                notifyDataSetChanged();
            }
        }
    }
    
    private static class ViewHolder {
        ImageView icon;
        TextView name;
        TextView description;
        TextView version;
        TextView category;
        TextView status;
        Button actionButton;
    }
}
