
SELECT
    p.ProductID,
    p.Name,
    Sum(si.Quantity) AS TotalUnitsSold

FROM Sale s
JOIN Sale_Item si ON si.SaleID = s.SaleID
JOIN Product p ON p.ProductID = si.ProductID
WHERE s.SaleDateTime >= datetime('now', '-30 days')
GROUP BY p.ProductID, p.Name
ORDER BY TotalUnitsSold DESC 
LIMIT 10;

BEGIN TRANSACTION;
INSERT INTO Sale (CustomerID, SaleDateTime, TotalAmount)
VALUES (1, datetime('now'), 18.99);

INSERT INTO Sale_Item (SaleId, ProductID, Quantity, UnitPriceAtSale)
VALUES (1, 1, 2, 6.99);

INSERT INTO Sale_Item (SaleId, ProductID, Quantity, UnitPriceAtSale)
VALUES (1, 2, 1, 4.99);

UPDATE Product SET StockQty = StockQty - 2 WHERE ProductID = 1;
UPDATE Product SET StockQty = StockQty - 1 WHERE ProductID = 2;

INSERT INTO Inventory_Adjustment (ProductID, AdjustmentDateTime, ChangeQty, Reason, Notes)
VALUES 
    (1, datetime('now'), -2, 'Sale', 'Decrease after sale = 1'),
    (2, datetime('now'), -1, 'Sale', 'Decrease after sale = 1');
COMMIT;


BEGIN TRANSACTION;
UPDATE Product
Set StockQty = StockQty + (
    SELECT SUM(Quantity)
    FROM Sale_Item
    WHERE SaleId = 1 AND Sale_Item.ProductID = Product.ProductID)

WHERE ProductID IN (
    SELECT ProductID
    FROM Sale_Item
    WHERE SaleID = 1
);
Insert INTO Inventory_Adjustment (ProductID, ChangeQty, Reason, Notes)
VALUES
    (1, datetime('now', -2), 'Sale Reversal', 'Voided SaleID = 1');

DELETE FROM Sale WHERE SaleID = 1;
COMMIT;


BEGIN TRANSACTION;
UPDATE Product
SET StockQty = StockQty - 2,
    IsActive = 1
WHERE ProductID = 1;

UPDATE Product
SET StockQty = StockQty - 1,
    IsActive = 1
WHERE ProductID = 2;

INSERT INTO Inventory_Adjustment (ProductID, AdjustmentDateTime, ChangeQty, Reason, Notes)
VALUES
    (1, datetime('now'), -2, 'Sale', 'Decrease after sale'),
    (2, datetime('now'), -1, 'Sale', 'Decrease after sale');
COMMIT;