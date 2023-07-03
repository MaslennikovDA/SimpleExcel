#include "cell.h"
#include "sheet.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <stack>

//базовый класс Impl для ячеек разных типов
class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual Value GetValue() const = 0;
    virtual std::string GetText() const = 0;
    virtual std::vector<Position> GetReferencedCells() const{
        return {};
    };
    virtual bool CacheOK() const {
        return true;
    }
    virtual void CacheReset() const {}
};
//пустая ячейка
class Cell::EmptyImpl : public Cell::Impl {
public:
    EmptyImpl(){}
    Value GetValue() const override {
        return "";
    }
    std::string GetText() const override {
        return "";
    }
};
//текстовая ячейка
class Cell::TextImpl : public Cell::Impl {
public:
    TextImpl(std::string text)
        :text_(text){}
    Value GetValue() const override {
        if (text_[0] == '\'') {//если апостроф, удаляем его
            std::string apost = text_;
            apost.erase(0, 1);
            return apost;
        }
        return text_;
    }
    std::string GetText() const override {
        return text_;
    }
private:
    std::string text_ = {};
};
//формульная ячейка
class Cell::FormulaImpl : public Cell::Impl {
public:
    FormulaImpl(std::string text, const SheetInterface& sheet)
        :sheet_(sheet)
    {
        std::string formula = text.erase(0, 1);
        formula_ = ParseFormula(formula);
    }
    Value GetValue() const override {
        if (cache_.has_value() == false) {
            std::variant<double, FormulaError> result = formula_->Evaluate(sheet_);
            if (std::holds_alternative<double>(result)) {
                cache_ = std::get<double>(result);
            }
            else {
                cache_ = std::get<FormulaError>(result);
            }
        }
        if (std::holds_alternative<double>(cache_.value())) {
            return std::get<double>(cache_.value());
        }
        else {
            return std::get<FormulaError>(cache_.value());
        }
    }
    std::string GetText() const override {
        return '='+formula_->GetExpression();
    }
    bool CacheOK() const override{
        return cache_.has_value();
    }
    void CacheReset() const override{
        cache_.reset();
    }
    std::vector<Position> GetReferencedCells() const override {
        return formula_->GetReferencedCells();
    }
private:
    std::unique_ptr<FormulaInterface> formula_;
    const SheetInterface& sheet_;
    mutable std::optional<FormulaInterface::Value> cache_;
};

// Реализуйте следующие методы
Cell::Cell(Sheet& sheet)
    :impl_(std::make_unique<EmptyImpl>()),
    sheet_(sheet){}

Cell::~Cell() {}

void Cell::Set(std::string text) {
    std::unique_ptr<Impl> newImpl;
    if (text.empty()) {
        newImpl = std::make_unique<EmptyImpl>();
    }
    else if (text.size() >1 && text[0] == '=') {
        newImpl = std::make_unique<FormulaImpl>(std::move(text), sheet_);
    }
    else {
        newImpl = std::make_unique<TextImpl>(std::move(text));
    }
    if (CircularDepend(*newImpl) == true) {
        throw CircularDependencyException("CircularDependencyException");
    }
    impl_ = std::move(newImpl);
    UpdateDepend();
    DeleteCache(true);
}

void Cell::Clear() {
    Set("");
}

Cell::Value Cell::GetValue() const {
    return impl_->GetValue();
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
    return !in_depend_.empty();
}

bool Cell::CircularDepend(const Impl& newImpl) {
    if (newImpl.GetReferencedCells().empty()) {
        return false;
    }
    std::unordered_set<const Cell*> all_depend;
    for (const Position position : newImpl.GetReferencedCells()) {
        all_depend.insert(static_cast<Cell*>(sheet_.GetCell(position)));
    }
    std::unordered_set<const Cell*> new_depend;
    std::stack<const Cell*> one_depend;
    one_depend.push(this);
    while (one_depend.empty() == false) {
        const Cell* cell_1 = one_depend.top();
        one_depend.pop();
        new_depend.insert(cell_1);
        if (all_depend.find(cell_1) != all_depend.end()) {
            return true;
        }
        for (const Cell* cell_2 : cell_1->in_depend_) {
            if (new_depend.find(cell_2) == new_depend.end()) {
                one_depend.push(cell_2);
            }
        }
    }
    return false;
}

void Cell::UpdateDepend() {
    for (Cell* cell : out_depend_) {
        cell->in_depend_.erase(this);
    }
    out_depend_.clear();
    for (const Position& position : impl_->GetReferencedCells()) {
        Cell* cell = static_cast<Cell*>(sheet_.GetCell(position));
        if (!cell) {
            sheet_.SetCell(position, "");
            cell = static_cast<Cell*>(sheet_.GetCell(position));
        }
        out_depend_.insert(cell);
        cell->in_depend_.insert(this);
    }
}

void Cell::DeleteCache(bool key) {
    if (impl_->CacheOK() == true || key == true) {
        impl_->CacheReset();
        for (Cell* cell : in_depend_) {
            cell->DeleteCache();
        }
    }
}