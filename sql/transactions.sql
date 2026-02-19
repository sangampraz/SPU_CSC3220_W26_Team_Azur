UPDATE Product
SET StockQty = StockQty - 1,
    IsActive = 1
WHERE ProductID = 1;
