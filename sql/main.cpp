#include <sqlite3.h>

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


// Database file created
static const string DB_FILE = "stockpilot.db";


namespace SQL {
    // Product queries
    const char* LIST_PRODUCTS =
        "SELECT ProductID, Name, UnitPrice, StockQty, LowStockThreshold, IsActive "
        "FROM Product "
        "ORDER BY ProductID;";

    // Insert a product
    const char* INSERT_PRODUCT =
        "INSERT INTO Product (SupplierID, Name, UnitPrice, StockQty, LowStockThreshold, IsActive) "
        "VALUES (?, ?, ?, ?, ?, 1);";
    
    // Fetches current stock for a single active product
    const char* GET_PRODUCT_STOCK =
        "SELECT StockQty FROM Product WHERE ProductID = ? AND IsActive = 1;";

    // Adjusts stock (positive or negative) for a single active product
    const char* UPDATE_PRODUCT_STOCK =
        "UPDATE Product SET StockQty = StockQty + ? WHERE ProductID = ? AND IsActive = 1;";

    // Inventory Adjustment Log
    // Logs an inventory adjustment with current timestamp
    const char* INSERT_ADJUSTMENT =
        "INSERT INTO Inventory_Adjustment (ProductID, AdjustmentDateTime, ChangeQty, Reason, Notes) "
        "VALUES (?, datetime('now'), ?, ?, ?);";

    // Sale queries
    // Creates a sale header
    const char* INSERT_SALE =
        "INSERT INTO Sale (CustomerID, SaleDateTime, TotalAmount) "
        "VALUES (?, datetime('now'), ?);";                              // uses datetime('now') for consistent timestamps

    // Inserts a sale line item
    const char* INSERT_SALE_ITEM =
        "INSERT INTO Sale_Item (SaleID, ProductID, Quantity, UnitPriceAtSale) "
        "VALUES (?, ?, ?, ?);";

    // Shows recent sales 
    const char* LIST_SALES =
        "SELECT SaleID, SaleDateTime, TotalAmount FROM Sale ORDER BY SaleID DESC LIMIT 20;";

    // Shows recent sales 
    const char* SALE_DETAILS =
        "SELECT si.ProductID, p.Name, si.Quantity, si.UnitPriceAtSale, (si.Quantity * si.UnitPriceAtSale) AS LineTotal "
        "FROM Sale_Item si "
        "JOIN Product p ON p.ProductID = si.ProductID "
        "WHERE si.SaleID = ?;";
}


/**
 * Clears the input stream state and discards the remainder of the current line
 * Prevents invalid input from breaking subsequent reads
 */
static void clearInput() {
    cin.clear();
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
}


/**
 * Reads a numeric value from the user with validation
 * Continues prompting until a valid value is entered
 */
template <typename T>
static T readNumber(const string& prompt) {
    while (true) {
        cout << prompt;
        T value{};
        if (cin >> value) {
            clearInput();                   // remove any extra characters on the line
            return value;
        }
        clearInput();                       // recover from invalid input
        cout << "Invalid input. Try again.\n";
    }
}


/**
 * Reads a full line of text from the user
 */
static string readLine(const string& prompt) {
    cout << prompt;
    string s;
    getline(cin, s);
    return s;
}


/**
 * Pause helper to keep console output readable
 */
static void pressEnter() {
    cout << "\nPress Enter to continue...";
    string _;
    getline(cin, _);
}



// DB WRAPPER

/**
 * Wrapper around sqlite3*
 * - opens database on construction
 * - close database on destruction
 */
class Db {
public:
    /**
    * Opens SQLite database file, throws error on failure
    * Enables foreign key enforcement for this connection 
    */
    explicit Db(const string& path) : db_(nullptr) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            string err = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw runtime_error("Failed to open database: " + err);
        }
        exec("PRAGMA foreign_keys = ON;");
    }


    /**
     * Ensures database handle is closed
     */
    ~Db() {
        if (db_) sqlite3_close(db_);
    }

    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;


    /**
     * Execures a SQL statement without parameters
     * throws runtime error if SQLite reports an error
     */
    void exec(const string& sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            string err = errmsg ? errmsg : "unknown error";
            sqlite3_free(errmsg);
            throw runtime_error("SQL exec error: " + err + "\nSQL: " + sql);
        }
    }


    /**
     * Prepares a parameterized SQL statement
     */
    sqlite3_stmt* prepare(const char* sql) {
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw runtime_error(string("Prepare failed: ") + sqlite3_errmsg(db_) + "\nSQL: " + sql);
        }
        return stmt;
    }


    /** Begins a transaction */
    void begin() { exec("BEGIN TRANSACTION;"); }

    /** Commits the current transaction */
    void commit() { exec("COMMIT;"); }

    /**
     * Atempts to roll back the current transaction
     */
    void rollback() noexcept {
        try { exec("ROLLBACK;"); } catch (...) {}
    }

    sqlite3* raw() { return db_; }

private:
    sqlite3* db_;
};


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


/**
 * Lists product in a formatted table and highlights low-stock and inactive status
 */
static void listProducts(Db& db) {
    sqlite3_stmt* stmt = db.prepare(SQL::LIST_PRODUCTS);
    cout << "\n=== PRODUCTS ===\n";
    cout << left
              << setw(10) << "ID"
              << setw(25) << "Name"
              << setw(12) << "Price"
              << setw(10) << "Stock"
              << setw(12) << "LowThreshold"
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


/**
 * Adds a new product to the database using a prepared INSERT statement
 */
static void addProduct(Db& db) {
    int supplierId = readNumber<int>("SupplierID (enter 0 for NULL): ");
    string name = readLine("Product name: ");
    double price = readNumber<double>("Unit price: ");
    int stock = readNumber<int>("Initial stock qty: ");
    int low = readNumber<int>("Low stock threshold: ");

    sqlite3_stmt* stmt = db.prepare(SQL::INSERT_PRODUCT);

    if (supplierId == 0) sqlite3_bind_null(stmt, 1);
    else sqlite3_bind_int(stmt, 1, supplierId);

    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, price);
    sqlite3_bind_int(stmt, 4, stock);
    sqlite3_bind_int(stmt, 5, low);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        string err = sqlite3_errmsg(db.raw());
        sqlite3_finalize(stmt);
        throw runtime_error("Insert product failed: " + err);
    }
    sqlite3_finalize(stmt);

    cout << "Product added.\n";
}


/**
 * Retrives current stock quantity for an active product
 */
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


/**
 * Adjusts product stock and logs the adjustment
 */
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


/**
 * Interactive inventory adjustment feature
 * Demonstrates a transaction that updates product and wirtes Inventory_Adjustment
 */
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
        cout << "Inventory updated & logged.\n";
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


/**
 * Creates a sale with multiple items
 */
static void createSale(Db& db) {
    cout << "\nCreate Sale: enter items. ProductID=0 to finish.\n";

    int customerId = readNumber<int>("CustomerID for this sale: ");

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
        sqlite3_bind_int(saleStmt, 1, customerId);
        sqlite3_bind_double(saleStmt, 2, total);

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
        cout << "Sale created. SaleID = " << saleId
                  << " Total=$" << fixed << setprecision(2) << total << "\n";
    } catch (...) {
        db.rollback();
        throw;
    }
}


/**
 * Shows recent sales and lets the user view details for a selected sale
 */
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

    cout << "\n--- Sale Details: SaleID = " << saleId << " ---\n";
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


/**
 * Resets the database file by deleting stockpilot.db and recreating it from SQL scripts
 * used the sqlite3 CLI to execute schema.sql and transactions.sql
 */
static void resetDbFromSqlFiles() {
    const string schema = "sql/schema.sql";
    const string seed   = "sql/transactions.sql";

    const string sqliteExe = ".\\sqlite\\sqlite3";

    if (!fs::exists(schema)) {
        cout << "schema.sql not found at: " << schema << "\n";
        return;
    }
    if (!fs::exists(sqliteExe)) {
        cout << "sqlite3.exe not found at: " << sqliteExe << "\n";
        return;
    }

    // Delete db file
    if (fs::exists(DB_FILE)) {
        fs::remove(DB_FILE);
    }

    // Build and run schema
    string cmd1 = sqliteExe + " " + DB_FILE + " < " + schema;
    int rc1 = system(cmd1.c_str());
    if (rc1 != 0) {
        cout << "Failed to run schema.sql.\n";
        return;
    }

    // Build and run seed/transactions
    if (fs::exists(seed)) {
        string cmd2 = sqliteExe + " " + DB_FILE + " < " + seed;
        int rc2 = system(cmd2.c_str());
        if (rc2 != 0) {
            cout << "Failed to run transactions.sql.\n";
            return;
        }
    } else {
        cout << "transactions.sql not found at: " << seed << "\n";
        return;
    }

    cout << "Database reset complete.\n";
}


/**
 * Prints the main console menu
 */
static void printMenu() {
    cout << "\n=============================\n";
    cout << "          StockPilot\n";
    cout << "=============================\n";
    cout << "1) List products\n";
    cout << "2) Add product\n";
    cout << "3) Restock / adjust inventory\n";
    cout << "4) Create sale (multi-item)\n";
    cout << "5) View sales + details\n";
    cout << "6) Reset DB from schema.sql/transactions.sql (optional)\n";
    cout << "0) Exit\n";
}


/**
 * Application entry point
 * Opens the database once and then loops the user menu until exit
 */
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
                cout << "Error: " << e.what() << "\n";
                pressEnter();
            }
        }

    } catch (const exception& e) {
        cout << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}