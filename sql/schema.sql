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

