#include "compiler.h"
#include <QHash>
#include <QRegularExpression>
#include <QSet>

namespace {
QString inferType(const QString &v) {
    const QString s=v.trimmed();
    if (s=="True"||s=="False") return "bool";
    if ((s.startsWith('"')&&s.endsWith('"'))||(s.startsWith('\'')&&s.endsWith('\''))) return "string";
    if (QRegularExpression("^-?\\d+$").match(s).hasMatch()) return "BigInteger";
    if (QRegularExpression("^-?\\d+\\.\\d+$").match(s).hasMatch()) return "double";
    return "dynamic";
}
}

CompilerResult analyzePython(const QString &source)
{
    CompilerResult r;
    QHash<QString,CompilerSymbol> symbols;
    QSet<QString> keywords={"def","return","if","elif","else","while","for","in","range","print","True","False","None","and","or","not"};
    QStringList tac;
    int temp=1,label=1,previousIndent=0;
    const QStringList lines=source.split('\n');
    for(int li=0;li<lines.size();++li){
        const QString raw=lines.at(li); const QString line=raw.trimmed(); if(line.isEmpty()) continue;
        int spaces=0; while(spaces<raw.size()&&raw.at(spaces)==' ') spaces++;
        if(spaces%4) r.syntaxErrors<<QString("Linea %1: indentacion invalida; use grupos de 4 espacios.").arg(li+1);
        const int ind=spaces/4;
        if(ind>previousIndent+1) r.syntaxErrors<<QString("Linea %1: salto de indentacion inesperado.").arg(li+1);
        previousIndent=ind;
        if((QRegularExpression("^(def|if|elif|else|while|for)\\b").match(line).hasMatch())&&!line.endsWith(':'))
            r.syntaxErrors<<QString("Linea %1: falta ':' al final del bloque.").arg(li+1);
        int balance=0; for(QChar c:line){if(c=='(')balance++;if(c==')')balance--;}
        if(balance!=0) r.syntaxErrors<<QString("Linea %1: parentesis desbalanceados.").arg(li+1);

        QRegularExpression tokenRx("(?:\"(?:\\\\.|[^\"])*\"|'(?:\\\\.|[^'])*'|\\d+\\.\\d+|\\d+|[A-Za-z_]\\w*|==|!=|<=|>=|\\*\\*|//|[-+*/%=<>():,])");
        auto it=tokenRx.globalMatch(line);
        while(it.hasNext()){
            QString x=it.next().captured(); QString type;
            if(keywords.contains(x)) type="PALABRA RESERVADA";
            else if(QRegularExpression("^[A-Za-z_]\\w*$").match(x).hasMatch()) type="IDENTIFICADOR";
            else if(QRegularExpression("^\\d+\\.\\d+$").match(x).hasMatch()) type="REAL";
            else if(QRegularExpression("^\\d+$").match(x).hasMatch()) type="ENTERO";
            else if(x.startsWith('"')||x.startsWith('\'')) type="CADENA";
            else if(QString("():,").contains(x)) type="DELIMITADOR"; else type="OPERADOR";
            r.tokens.append({x,type,li+1});
        }

        QRegularExpressionMatch m=QRegularExpression("^def\\s+(\\w+)\\s*\\(([^)]*)\\)\\s*:$").match(line);
        if(m.hasMatch()){
            const QString name=m.captured(1); if(symbols.contains(name)) r.semanticErrors<<QString("Linea %1: '%2' ya fue declarado.").arg(li+1).arg(name);
            else {CompilerSymbol s{name,"dynamic","funcion","global",li+1};symbols.insert(name,s);r.symbols<<s;}
            for(QString p:m.captured(2).split(',',Qt::SkipEmptyParts)){p=p.trimmed();CompilerSymbol s{p,"dynamic","parametro",name,li+1};r.symbols<<s;}
            tac<<QString("func %1:").arg(name); continue;
        }
        m=QRegularExpression("^(\\w+)\\s*=\\s*(.+)$").match(line);
        if(m.hasMatch()){
            const QString name=m.captured(1),expr=m.captured(2); const QString type=inferType(expr);
            if(!symbols.contains(name)){CompilerSymbol s{name,type,"variable",ind?"local":"global",li+1};symbols.insert(name,s);r.symbols<<s;}
            else if(symbols[name].type!="dynamic"&&type!="dynamic"&&symbols[name].type!=type)
                r.semanticErrors<<QString("Linea %1: asignacion incompatible; %2 es %3 y recibe %4.").arg(li+1).arg(name,symbols[name].type,type);
            tac<<QString("t%1 = %2").arg(temp++).arg(expr)<<QString("%1 = t%2").arg(name).arg(temp-1); continue;
        }
        m=QRegularExpression("^print\\((.*)\\)$").match(line);
        if(m.hasMatch()){tac<<"param "+m.captured(1)<<"call print, 1";continue;}
        m=QRegularExpression("^(if|elif|while)\\s+(.+)\\s*:$").match(line);
        if(m.hasMatch()){tac<<QString("ifFalse %1 goto L%2").arg(m.captured(2)).arg(label++);continue;}
        m=QRegularExpression("^for\\s+(\\w+)\\s+in\\s+range\\((.*)\\)\\s*:$").match(line);
        if(m.hasMatch()){if(!symbols.contains(m.captured(1))){CompilerSymbol s{m.captured(1),"int","variable","ciclo",li+1};symbols.insert(s.name,s);r.symbols<<s;}tac<<QString("%1 = inicio_range").arg(m.captured(1))<<QString("L%1:").arg(label++);continue;}
        m=QRegularExpression("^return(?:\\s+(.+))?$").match(line); if(m.hasMatch()){tac<<"return "+m.captured(1);continue;}

        QRegularExpression idRx("\\b([A-Za-z_]\\w*)\\b"); auto ids=idRx.globalMatch(line);
        while(ids.hasNext()){QString id=ids.next().captured(1);if(!keywords.contains(id)&&!symbols.contains(id))r.semanticErrors<<QString("Linea %1: identificador '%2' no declarado.").arg(li+1).arg(id);}
    }
    r.intermediateCode=tac.join('\n');
    return r;
}
