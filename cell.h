#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>

class Sheet;

class Cell : public CellInterface {
public:
    Cell(Sheet& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    bool IsReferenced() const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    std::unique_ptr<Impl> impl_;
    Sheet& sheet_;
//поля для реализации графа зависимостей
    std::unordered_set<Cell*> in_depend_;
    std::unordered_set<Cell*> out_depend_;
//метод проверки циклических ссылок
    bool CircularDepend(const Impl& newImpl);
//метод обновления графа зависимостей
    void UpdateDepend();
//метод удаления кэша вычислений
    void DeleteCache(bool key = false);
};