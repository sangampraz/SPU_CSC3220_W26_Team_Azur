
SELECT
    p.ProductID,
    p.Name,
    Sum(si.Quantity) AS TotalUnitsSold

FROM Sales s
JOIN Sale_Item si ON si.SaleID = s.SaleID
JOIN Product p ON p.ProductID = si.ProductID
WHERE s.SaleDateTime >= datetime('now', '-30 days')
GROUP BY p.ProductID, p.Name
ORDER BY TotalUnitsSold DESC 
LIMIT 10;

BEGIN TRANSACTION;

INSERT INTO Sale (CustomerID, SaleDateTime, TotalAMount)
VALUES (1, CURRENT_TIMESTASMP, 18.99);

INSERT INTO Sale_Item (SaleId, ProductID, Quantity, UnitPriceAtSale)
VALUES (last_insert_rowid(), 1, 2, 6.99);

INSERT INTO Sale_Item (SaleId, ProductID, Quantity, UnitPriceAtSale)
VALUES (last_insert_rowid(), 2, 1, 4.99);

UPDATE Product SET StockQty = StockQty - 2 WHERE ProductID = 1;
UPDATE Product SET StockQty = StockQty - 1 WHERE ProductID = 2;

INSERT INTO Inventory_Adjustment (ProductID, ChangeQty, Reason, Notes)
VALUES 
    (1, -2, 'Sale', 'Decrease after sale'),
    (2, -1, 'Sale', 'Decrease after sale');

COMMIT;
    