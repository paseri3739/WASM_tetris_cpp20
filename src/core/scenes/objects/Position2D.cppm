export module Position2D;

export struct Position2D {
    const int x;
    const int y;
    Position2D(int x_val, int y_val) : x(x_val), y(y_val) {}
};

export struct ColumnRow {
    const int column;
    const int row;
    ColumnRow(int col, int r) : column(col), row(r) {}
};
