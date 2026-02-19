CREATE TABLE Sale (
  SaleID INTEGER PRIMARY KEY,
  CustomerID INTEGER NOT NULL,
  SaleDateTime TEXT NOT NULL,
  TotalAmount REAL NOT NULL, 
FORIEGN KEY (CustomerID) REFRENCES Customer(CustomerID)
  );
