#pragma once
#include <QList>
#include <QString>
#include <QStringList>

struct CompilerToken { QString lexeme; QString type; int line; };
struct CompilerSymbol { QString name; QString type; QString category; QString scope; int line; };
struct CompilerResult {
    QList<CompilerToken> tokens;
    QList<CompilerSymbol> symbols;
    QStringList lexicalErrors;
    QStringList syntaxErrors;
    QStringList semanticErrors;
    QString intermediateCode;
    bool success() const { return lexicalErrors.isEmpty() && syntaxErrors.isEmpty() && semanticErrors.isEmpty(); }
};
CompilerResult analyzePython(const QString &source);
