-- Seed data
PRAGMA foreign_keys = ON;

INSERT INTO Customer (CustomerID, FirstName, LastName)
VALUES (1, 'David', 'Hernandes');

INSERT INTO Supplier (SupplierName) VALUES ('Costco');
INSERT INTO Supplier (SupplierName) VALUES ('US Foods');

INSERT INTO Product (ProductID, SupplierID, Name, UnitPrice, StockQty, LowStockThreshold, IsActive)
VALUES (1, 1, 'Flour (50lb)', 24.99, 10, 2, 1);
INSERT INTO Product (ProductID, SupplierID, Name, UnitPrice, StockQty, LowStockThreshold, IsActive)
VALUES (2, 1, 'Cheese (5lb)', 14.99, 12, 3, 1);
INSERT INTO Product (ProductID, SupplierID, Name, UnitPrice, StockQty, LowStockThreshold, IsActive)
VALUES (3, 2, 'Pepperoni', 9.99, 20, 5, 1);
INSERT INTO Product (ProductID, SupplierID, Name, UnitPrice, StockQty, LowStockThreshold, IsActive)
VALUES (4, 2, 'Tomato Sauce', 6.49, 12, 4, 1);