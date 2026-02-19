BEGIN TRANSACTION;
DELETE FROM Sale_Item
WHERE SaleID = 1;
DELETE FROM Sale
WHERE SaleID = 1;
COMMIT;

UPDATE Product
SET StockQty = StockQty - 1,
    IsActive = 1
WHERE ProductID = 1;