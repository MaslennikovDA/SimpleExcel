#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

FormulaError::FormulaError(Category category) {
    category_ = category;
}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    switch (category_) {
    case Category::Ref:
        return "#REF!";
    case Category::Value:
        return "#VALUE!";
    case Category::Div0:
        return "#DIV/0!";
    }
    return "";
}
std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << fe.ToString();
}

namespace {
    double GetDoubleFrom(const std::string& str) {
        double value = 0;
        if (!str.empty()) {
            std::istringstream in(str);
            if (!(in >> value) || !in.eof()) {
                throw FormulaError(FormulaError::Category::Value);
            }
        }
        return value;
    }

    double GetDoubleFrom(double value) {
        return value;
    }

    double GetDoubleFrom(FormulaError error) {
        throw FormulaError(error);
    }

    double GetCellValue(const CellInterface* cell) {
        if (!cell) return 0;
        return std::visit([](const auto& value) { return GetDoubleFrom(value); },
            cell->GetValue());
    }
}

namespace {
class Formula : public FormulaInterface {
public:
//Объект класса Formula создаётся в методе ParseFormula:std::unique_ptr<FormulaInterface> ParseFormula(std::string expression); .
//Если строка expression содержит синтаксически некорректную формулу, метод должен кидать исключение FormulaException
    explicit Formula(std::string expression)
    try
        :ast_(ParseFormulaAST(expression)){}
    catch (const std::exception& exc) {
        std::throw_with_nested(FormulaException(exc.what()));
    }
//Метод Evaluate() должен вычислять значение формулы и возвращать число, если формулу удалось вычислить.
//Если не удалось, должен возвращать ошибку вычисления FormulaError. Для этого в файле common.h объявлен метод вывода ошибки в поток, а в файле formula.cpp он реализован.
    Value Evaluate(const SheetInterface& sheet) const override {
        try {
            auto cell_lookup = [&sheet](Position position) -> double {
                if (!position.IsValid()) {
                    throw FormulaError(FormulaError::Category::Ref);
                }
                const auto* cell = sheet.GetCell(position);
                return GetCellValue(cell);
            };

            return ast_.Execute(cell_lookup);
        }
        catch (FormulaError error) {
            // NOTE(a-square): in production, using exceptions in a potentially hot
            // path is problematic, instead we would remove recursion in favor of a
            // manual stack machine with early exit on encountering formula errors
            return error;
        }
    }
//В методе вывода формулы в поток GetExpression() используйте «очищенную» формулу без лишних
//скобок из FormulaAST::PrintFormula(std::ostream& out).
    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        std::vector<Position> cells;
        for (const Position cell: ast_.GetCells()) {
            if (cell.IsValid()) {
                cells.push_back(cell);
            }
        }
        cells.resize(std::unique(cells.begin(), cells.end()) - cells.begin());
        return cells;
    }

private:
    FormulaAST ast_;
};
}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}