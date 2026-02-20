
PRAGMA foreign_keys = ON;

-- Customer
CREATE TABLE Customer (
    CustomerID  INTEGER PRIMARY KEY,
    FirstName   TEXT NOT NULL,
    LastName    TEXT NOT NULL,
    Phone       TEXT,
    Email       TEXT,
    Notes       TEXT 
);

-- Supplier
CREATE TABLE Supplier (
    SupplierID      INTEGER PRIMARY KEY AUTOINCREMENT,
    SupplierName    TEXT NOT NULL,
    Phone           TEXT,
    Email           TEXT,
    Address         TEXT,
    Note            Text
);

-- Product
CREATE TABLE Product (
    ProductID           INTEGER PRIMARY KEY,
    SupplierID          INTEGER,
    Name                TEXT NOT NULL,
    Description         TEXT,
    SKU                 TEXT,
    UnitPrice           REAL NOT NULL CHECK (UnitPrice >= 0),
    CostPrice           REAL CHECK (CostPrice IS NULL OR CostPrice >= 0),
    StockQty            INTEGER NOT NULL CHECK (StockQty >= 0),
    LowStockThreshold   INTEGER NOT NULL CHECK (LowStockThreshold >= 9),
    ReorderQty          INTEGER CHECK (ReorderQty IS NULL OR ReorderQty >= 0),
    IsActive            INTEGER NOT NULL DEFAULT 1 CHECK (IsActive IN (0,1)),

CONSTRAINT UQ_Product_SKU UNIQUE (SKU),
CONSTRAINT FK_Product_Supplier
    FOREIGN KEY (SupplierID) REFERENCES Supplier(SupplierID)
    ON UPDATE CASCADE
    ON DELETE SET NULL
);

-- Sale
CREATE TABLE Sale (
  SaleID            INTEGER PRIMARY KEY,
  CustomerID        INTEGER NOT NULL,
  SaleDateTime      TEXT NOT NULL,
  TotalAmount       REAL NOT NULL, 
  FOREIGN KEY (CustomerID) REFERENCES Customer(CustomerID)
  );

-- Sale_Item
CREATE TABLE Sale_Item (
    SaleItemID          INTEGER PRIMARY KEY AUTOINCREMENT,
    SaleID              INTEGER NOT NULL,
    ProductID           INTEGER NOT NULL,
    Quantity            INTEGER NOT NULL (Quantity > 0),
    UnitPriceAtSale     REAL NOT NULL (UnitPriceAtSale >= 0),
    FOREIGN KEY (SaleID) REFERENCES Sale(SaleID) ON DELETE CASCADE,
    FOREIGN KEY (ProductID) REFERENCES Product(ProductID)
);

-- Inventory_Adjustment
CREATE TABLE Inventory_Adjustment (
  AdjustmentID INTEGER PRIMARY KEY,
  ProductID INTEGER NOT NULL,
  AdjustmentDateTime TEXT NOT NULL,
  ChangeQty INTEGER NOT NULL,
  Reason TEXT,
  Notes TEXT,
  FOREIGN KEY (ProductID) REFERENCES Product(ProductID)
  );





