// DDL
PRAGMA foreign_keys = ON;

CREATE TABLE Customer (
    CustomerID  INTEGER PRIMARY KEY,
    FirstName   TEXT NOT NULL,
    LastName    TEXT NOT NULL,
    Phone       TEXT,
    Email       TEXT,
    Notes       TEXT 
);

CREATE TABLE Product (
    ProductID           INTEGER PRIMARY KEY,
    SupplierID          INTEGER,
    LastName            Text NOT NULL,
    Description         TEXT,
    SKU                 TEXT,
    UnitPrice           REAL NOT NULL CHECK (UnitPrice >= 0),
    CostPrice           REAL CHECK (CostPrice IS NULL OR CostPRice >= 0),
    StockQty            INTEGER NOT NULL CHECK (StockQty >= 0),
    LowStockThreshold   INTEGER NOT NULL CHECK (LowStockThreshold >= 9),
    ReorderQty          INTEGER CHECK (ReorderQty IS NULL OR ReorderQty >= 0),
    IsActive            INTEGER NOT NULL DEFAULT 1 CHECK (IsActive IN (0,1)),

CONSTRAINTS UQ_Product_SKU UNIQUE (SKU),
CONSTRAINTS FK_Product_Supplier
    FOREIGN KEY (SupplierID) REFERENCES Supplier(SupplierID)
    ON UPDATE CASCADE
    ON DELETE SET NULL
);