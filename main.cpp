#include <sqlite3.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

using namespace std;
namespace fs = filesystem;


static const string DB_FILE = "stockpilot.db";


namespace SQL {
    // Product
    const char* LIST_PRODUCTS =
        "SELECT ProductID, Name, UnitPrice, StockQty, LowStockThreshold, IsActive "
        "FROM Product "
        "ORDER BY ProductID;";

    const char* INSERT_PRODUCT =
        "INSERT INTO Product (Name, UnitPrice, StockQty, LowStockThreshold, IsActive) "
        "VALUES (?, ?, ?, ?, 1);";

    const char* GET_PRODUCT_STOCK =
        "SELECT StockQty FROM Product WHERE ProductID = ? AND IsActive = 1;";

    const char* UPDATE_PRODUCT_STOCK =
        "UPDATE Product SET StockQty = StockQty + ? WHERE ProductID = ? AND IsActive = 1;";

    // Inventory Adjustment
    const char* INSERT_ADJUSTMENT =
        "INSERT INTO Inventory_Adjustment (ProductID, ChangeQty, Reason, Notes) "
        "VALUES (?, ?, ?, ?);";

    // Sale
    const char* INSERT_SALE =
        "INSERT INTO Sale (SaleDate, Total) VALUES (datetime('now'), ?);";

    const char* INSERT_SALE_ITEM =
        "INSERT INTO Sale_Item (SaleID, ProductID, Quantity, UnitPrice) "
        "VALUES (?, ?, ?, ?);";

    const char* LIST_SALES =
        "SELECT SaleID, SaleDate, Total FROM Sale ORDER BY SaleID DESC LIMIT 20;";

    const char* SALE_DETAILS =
        "SELECT si.ProductID, p.Name, si.Quantity, si.UnitPrice, (si.Quantity * si.UnitPrice) AS LineTotal "
        "FROM Sale_Item si "
        "JOIN Product p ON p.ProductID = si.ProductID "
        "WHERE si.SaleID = ?;";
}

// =======================
// SMALL UTILS
// =======================
static void clearInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}

template <typename T>
static T readNumber(const string& prompt) {
    while (true) {
        cout << prompt;
        T value{};
        if (cin >> value) {
            clearInput();
            return value;
        }
        clearInput();
        cout << "Invalid input. Try again.\n";
    }
}

static string readLine(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    return s;
}

static void pressEnter() {
    cout << "\nPress Enter to continue...";
    string _;
    getline(cin, _);
}

// =======================
// DB WRAPPER
// =======================
class Db {
public:
    explicit Db(const string& path) : db_(nullptr) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            string err = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw runtime_error("Failed to open database: " + err);
        }
        exec("PRAGMA foreign_keys = ON;");
    }

    ~Db() {
        if (db_) sqlite3_close(db_);
    }

    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    void exec(const string& sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            string err = errmsg ? errmsg : "unknown error";
            sqlite3_free(errmsg);
            throw runtime_error("SQL exec error: " + err + "\nSQL: " + sql);
        }
    }

    sqlite3_stmt* prepare(const char* sql) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw runtime_error(string("Prepare failed: ") + sqlite3_errmsg(db_) + "\nSQL: " + sql);
        }
        return stmt;
    }

    void begin() { exec("BEGIN TRANSACTION;"); }
    void commit() { exec("COMMIT;"); }
    void rollback() noexcept {
        try { exec("ROLLBACK;"); } catch (...) {}
    }

    sqlite3* raw() { return db_; }

private:
    sqlite3* db_;
};

// =======================
// PRINTING HELPERS
// =======================
static void printRow(sqlite3_stmt* stmt) {
    int cols = sqlite3_column_count(stmt);
    for (int i = 0; i < cols; i++) {
        const char* name = sqlite3_column_name(stmt, i);
        const unsigned char* txt = sqlite3_column_text(stmt, i);
        cout << name << "=" << (txt ? reinterpret_cast<const char*>(txt) : "NULL");
        if (i < cols - 1) cout << " | ";
    }
    cout << "\n";
}

// =======================
// FEATURES (MVP)
// =======================
static void listProducts(Db& db) {
    sqlite3_stmt* stmt = db.prepare(SQL::LIST_PRODUCTS);
    cout << "\n=== PRODUCTS ===\n";
    cout << left
              << setw(10) << "ID"
              << setw(25) << "Name"
              << setw(12) << "Price"
              << setw(10) << "Stock"
              << setw(12) << "LowThres"
              << setw(10) << "Active"
              << "Status\n";
    cout << string(90, '-') << "\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        double price = sqlite3_column_double(stmt, 2);
        int stock = sqlite3_column_int(stmt, 3);
        int low = sqlite3_column_int(stmt, 4);
        int active = sqlite3_column_int(stmt, 5);

        string status = "";
        if (!active) status = "INACTIVE";
        else if (stock <= low) status = "LOW STOCK";

        cout << left
                  << setw(10) << id
                  << setw(25) << (name ? reinterpret_cast<const char*>(name) : "NULL")
                  << setw(12) << fixed << setprecision(2) << price
                  << setw(10) << stock
                  << setw(12) << low
                  << setw(10) << active
                  << status << "\n";
    }

    sqlite3_finalize(stmt);
}

static void addProduct(Db& db) {
    string name = readLine("Product name: ");
    double price = readNumber<double>("Unit price: ");
    int stock = readNumber<int>("Initial stock qty: ");
    int low = readNumber<int>("Low stock threshold: ");

    sqlite3_stmt* stmt = db.prepare(SQL::INSERT_PRODUCT);
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 2, price);
    sqlite3_bind_int(stmt, 3, stock);
    sqlite3_bind_int(stmt, 4, low);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        string err = sqlite3_errmsg(db.raw());
        sqlite3_finalize(stmt);
        throw runtime_error("Insert product failed: " + err);
    }
    sqlite3_finalize(stmt);

    cout << "✅ Product added.\n";
}

static int getStock(Db& db, int productId) {
    sqlite3_stmt* stmt = db.prepare(SQL::GET_PRODUCT_STOCK);
    sqlite3_bind_int(stmt, 1, productId);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return numeric_limits<int>::min(); // not found
    }
    int stock = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return stock;
}

static void adjustStock(Db& db, int productId, int deltaQty, const string& reason, const string& notes) {
    // Update stock
    sqlite3_stmt* up = db.prepare(SQL::UPDATE_PRODUCT_STOCK);
    sqlite3_bind_int(up, 1, deltaQty);
    sqlite3_bind_int(up, 2, productId);

    if (sqlite3_step(up) != SQLITE_DONE) {
        string err = sqlite3_errmsg(db.raw());
        sqlite3_finalize(up);
        throw runtime_error("Update stock failed: " + err);
    }
    int changed = sqlite3_changes(db.raw());
    sqlite3_finalize(up);

    if (changed == 0) {
        throw runtime_error("No product updated. Check ProductID or IsActive.");
    }

    // Log adjustment
    sqlite3_stmt* adj = db.prepare(SQL::INSERT_ADJUSTMENT);
    sqlite3_bind_int(adj, 1, productId);
    sqlite3_bind_int(adj, 2, deltaQty);
    sqlite3_bind_text(adj, 3, reason.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(adj, 4, notes.c_str(), -1, SQLITE_TRANSIENT);

    if (sqlite3_step(adj) != SQLITE_DONE) {
        string err = sqlite3_errmsg(db.raw());
        sqlite3_finalize(adj);
        throw runtime_error("Insert adjustment failed: " + err);
    }
    sqlite3_finalize(adj);
}

static void restockProduct(Db& db) {
    int productId = readNumber<int>("ProductID to adjust: ");
    int delta = readNumber<int>("Change quantity (positive restock, negative shrink): ");
    string reason = readLine("Reason (e.g., Restock, Damage, Correction): ");
    string notes = readLine("Notes (optional): ");

    db.begin();
    try {
        int stock = getStock(db, productId);
        if (stock == numeric_limits<int>::min()) {
            throw runtime_error("Product not found or inactive.");
        }
        if (stock + delta < 0) {
            throw runtime_error("Adjustment would make stock negative. Cancelled.");
        }

        adjustStock(db, productId, delta, reason, notes);
        db.commit();
        cout << "✅ Inventory updated & logged.\n";
    } catch (...) {
        db.rollback();
        throw;
    }
}

struct SaleItemInput {
    int productId{};
    int qty{};
    double unitPrice{};
};

static void createSale(Db& db) {
    cout << "\nCreate Sale: enter items. ProductID=0 to finish.\n";

    vector<SaleItemInput> items;
    while (true) {
        int pid = readNumber<int>("ProductID: ");
        if (pid == 0) break;
        int qty = readNumber<int>("Quantity: ");
        if (qty <= 0) {
            cout << "Quantity must be > 0.\n";
            continue;
        }
        double price = readNumber<double>("Unit price at sale: ");
        if (price < 0) {
            cout << "Price must be >= 0.\n";
            continue;
        }
        items.push_back({pid, qty, price});
    }

    if (items.empty()) {
        cout << "No items entered. Sale cancelled.\n";
        return;
    }

    // Calculate total
    double total = 0.0;
    for (const auto& it : items) total += (it.unitPrice * it.qty);

    db.begin();
    try {
        // Validate stock first
        for (const auto& it : items) {
            int stock = getStock(db, it.productId);
            if (stock == numeric_limits<int>::min()) {
                throw runtime_error("ProductID " + to_string(it.productId) + " not found or inactive.");
            }
            if (stock < it.qty) {
                ostringstream oss;
                oss << "Insufficient stock for ProductID " << it.productId
                    << ". Have " << stock << ", need " << it.qty << ".";
                throw runtime_error(oss.str());
            }
        }

        // Insert Sale
        sqlite3_stmt* saleStmt = db.prepare(SQL::INSERT_SALE);
        sqlite3_bind_double(saleStmt, 1, total);

        if (sqlite3_step(saleStmt) != SQLITE_DONE) {
            string err = sqlite3_errmsg(db.raw());
            sqlite3_finalize(saleStmt);
            throw runtime_error("Insert sale failed: " + err);
        }
        sqlite3_finalize(saleStmt);

        sqlite3_int64 saleId = sqlite3_last_insert_rowid(db.raw());

        // Insert Sale Items + Update stock + Log adjustment
        for (const auto& it : items) {
            // sale_item
            sqlite3_stmt* si = db.prepare(SQL::INSERT_SALE_ITEM);
            sqlite3_bind_int64(si, 1, saleId);
            sqlite3_bind_int(si, 2, it.productId);
            sqlite3_bind_int(si, 3, it.qty);
            sqlite3_bind_double(si, 4, it.unitPrice);

            if (sqlite3_step(si) != SQLITE_DONE) {
                string err = sqlite3_errmsg(db.raw());
                sqlite3_finalize(si);
                throw runtime_error("Insert sale_item failed: " + err);
            }
            sqlite3_finalize(si);

            // stock change = -qty
            string reason = "Sale";
            string notes = "SaleID=" + to_string(saleId);
            adjustStock(db, it.productId, -it.qty, reason, notes);
        }

        db.commit();
        cout << "✅ Sale created. SaleID=" << saleId
                  << " Total=$" << fixed << setprecision(2) << total << "\n";
    } catch (...) {
        db.rollback();
        throw;
    }
}

static void viewSales(Db& db) {
    sqlite3_stmt* stmt = db.prepare(SQL::LIST_SALES);
    cout << "\n=== RECENT SALES (last 20) ===\n";
    cout << left
              << setw(10) << "SaleID"
              << setw(22) << "SaleDate"
              << setw(12) << "Total"
              << "\n";
    cout << string(46, '-') << "\n";

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int saleId = sqlite3_column_int(stmt, 0);
        const unsigned char* date = sqlite3_column_text(stmt, 1);
        double total = sqlite3_column_double(stmt, 2);

        cout << left
                  << setw(10) << saleId
                  << setw(22) << (date ? reinterpret_cast<const char*>(date) : "NULL")
                  << setw(12) << fixed << setprecision(2) << total
                  << "\n";
    }

    sqlite3_finalize(stmt);

    int saleId = readNumber<int>("Enter SaleID to view details (0 to return): ");
    if (saleId == 0) return;

    sqlite3_stmt* det = db.prepare(SQL::SALE_DETAILS);
    sqlite3_bind_int(det, 1, saleId);

    cout << "\n--- Sale Details: SaleID=" << saleId << " ---\n";
    cout << left
              << setw(10) << "ProdID"
              << setw(25) << "Name"
              << setw(10) << "Qty"
              << setw(12) << "UnitPrice"
              << setw(12) << "LineTotal"
              << "\n";
    cout << string(70, '-') << "\n";

    bool any = false;
    double sum = 0.0;
    while (sqlite3_step(det) == SQLITE_ROW) {
        any = true;
        int pid = sqlite3_column_int(det, 0);
        const unsigned char* name = sqlite3_column_text(det, 1);
        int qty = sqlite3_column_int(det, 2);
        double unitPrice = sqlite3_column_double(det, 3);
        double lineTotal = sqlite3_column_double(det, 4);
        sum += lineTotal;

        cout << left
                  << setw(10) << pid
                  << setw(25) << (name ? reinterpret_cast<const char*>(name) : "NULL")
                  << setw(10) << qty
                  << setw(12) << fixed << setprecision(2) << unitPrice
                  << setw(12) << fixed << setprecision(2) << lineTotal
                  << "\n";
    }

    sqlite3_finalize(det);

    if (!any) {
        cout << "No items found for that SaleID.\n";
    } else {
        cout << "------------------------------\n";
        cout << "Computed Total: $" << fixed << setprecision(2) << sum << "\n";
    }
}

static void resetDbFromSqlFiles() {
    // Optional: if you keep schema.sql and seed.sql next to the program.
    // This function uses system() to call sqlite3. Works great for demo resets.
    // If you don't want this feature, you can remove it.
    const string schema = "schema.sql";
    const string seed = "seed.sql";

    if (!fs::exists(schema)) {
        cout << "schema.sql not found. Put it next to the executable.\n";
        return;
    }

    // Delete db file
    if (fs::exists(DB_FILE)) {
        fs::remove(DB_FILE);
    }

    string cmd1 = "sqlite3 " + DB_FILE + " < " + schema;
    int rc1 = system(cmd1.c_str());
    if (rc1 != 0) {
        cout << "Failed to run schema.sql (is sqlite3 installed?).\n";
        return;
    }

    if (fs::exists(seed)) {
        string cmd2 = "sqlite3 " + DB_FILE + " < " + seed;
        int rc2 = system(cmd2.c_str());
        if (rc2 != 0) {
            cout << "Failed to run seed.sql.\n";
            return;
        }
    }

    cout << "✅ Database reset complete.\n";
}

// =======================
// MAIN MENU
// =======================
static void printMenu() {
    cout << "\n=============================\n";
    cout << " StockPilot MVP (SQLite + C++)\n";
    cout << "=============================\n";
    cout << "1) List products\n";
    cout << "2) Add product\n";
    cout << "3) Restock / adjust inventory\n";
    cout << "4) Create sale (multi-item)\n";
    cout << "5) View sales + details\n";
    cout << "6) Reset DB from schema.sql/seed.sql (optional)\n";
    cout << "0) Exit\n";
}

int main() {
    try {
        Db db(DB_FILE);

        while (true) {
            printMenu();
            int choice = readNumber<int>("Choose: ");

            try {
                switch (choice) {
                    case 1:
                        listProducts(db);
                        pressEnter();
                        break;
                    case 2:
                        addProduct(db);
                        pressEnter();
                        break;
                    case 3:
                        restockProduct(db);
                        pressEnter();
                        break;
                    case 4:
                        createSale(db);
                        pressEnter();
                        break;
                    case 5:
                        viewSales(db);
                        pressEnter();
                        break;
                    case 6:
                        resetDbFromSqlFiles();
                        pressEnter();
                        break;
                    case 0:
                        cout << "Bye.\n";
                        return 0;
                    default:
                        cout << "Invalid option.\n";
                        break;
                }
            } catch (const exception& e) {
                cout << "❌ Error: " << e.what() << "\n";
                pressEnter();
            }
        }

    } catch (const exception& e) {
        cout << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}