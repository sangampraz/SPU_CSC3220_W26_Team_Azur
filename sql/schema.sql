CREATE TABLE Sale (
  SaleID INTEGER PRIMARY KEY,
  CustomerID INTEGER NOT NULL,
  SaleDateTime TEXT NOT NULL,
  TotalAmount REAL NOT NULL, 
FORIEGN KEY (CustomerID) REFRENCES Customer(CustomerID)
  );

CREATE TABLE Inventory_Adjustment (
  AdjustmentID INTEGER PRIMARY KEY,
  ProductID INTEGER NOT NULL,
  AdjustmentDateTime TEXT NOT NULL,
  ChangeQty INTEGER NOT NULL,
  Reason TEXT,
  Notes TEXT,
  FOREIGN KEY (ProductID) REFERENCES Product(ProductID)
  );

CREATE TABLE Supplier (
    SupplierID INTEGER PRIMARY KEY AUTOINCREMENT,
    SupplierName TEXT NOT NULL,
    Phone TEXT,
    Email TEXT,
    Address TEXT,
    Note Text
);

CREATE TABLE Sale_Item (
    SaleItemID INTEGER PRIMARY KEY AUTOINCREMENT,
    SaleID INTEGER NOT NULL,
    ProductID INTEGER NOT NULL,
    Quantity INTEGER NOT NULL,
    UnitPriceAtSale Real NOT NULL,
    FOREIGN KEY (SaleID) REFERENCES Sale(SaleID) ON DELETE CASCADE,
    FOREIGN KEY (ProductID) REFERENCES Product(ProductID)
);



