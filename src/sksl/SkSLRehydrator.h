/*
 * Copyright 2020 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_REHYDRATOR
#define SKSL_REHYDRATOR

#include "include/private/SkSLDefines.h"
#include "include/private/SkSLModifiers.h"
#include "include/private/SkSLSymbol.h"
#include "src/sksl/SkSLContext.h"
#include "src/sksl/ir/SkSLProgram.h"

#include <vector>

namespace SkSL {

class Compiler;
class Context;
class ErrorReporter;
class Expression;
class ProgramElement;
class Statement;
class SymbolTable;
class Type;

/**
 * Interprets a simple bytecode format that encodes the structure of an SkSL IR tree. This is used
 * to process the .sksl files representing SkSL's core include files, so that they can be quickly
 * reconstituted at runtime.
 */
class Rehydrator {
public:
    static constexpr uint16_t kVersion = 7;

    enum Command {
        // uint16 id, Type componentType, uint8 count
        kArrayType_Command,
        // Expression left, uint8 op, Expression right, Type type
        kBinary_Command,
        // SymbolTable symbolTable, uint8 statementCount, Statement[] statements, bool isScope
        kBlock_Command,
        // bool value
        kBoolLiteral_Command,
        kBreak_Command,
        // int16 builtin
        kBuiltinLayout_Command,
        // (All constructors) Type type, uint8 argCount, Expression[] arguments
        kConstructorArray_Command,
        kConstructorArrayCast_Command,
        kConstructorCompound_Command,
        kConstructorCompoundCast_Command,
        kConstructorDiagonalMatrix_Command,
        kConstructorMatrixResize_Command,
        kConstructorScalarCast_Command,
        kConstructorSplat_Command,
        kConstructorStruct_Command,
        kContinue_Command,
        kDefaultLayout_Command,
        kDefaultModifiers_Command,
        kDiscard_Command,
        // Statement stmt, Expression test
        kDo_Command,
        // ProgramElement[] elements (reads until command `kElementsComplete_Command` is found)
        kElements_Command,
        // no arguments--indicates end of Elements list
        kElementsComplete_Command,
        // Expression expression
        kExpressionStatement_Command,
        // uint16 ownerId, uint8 index
        kField_Command,
        // Expression base, uint8 index, uint8 ownerKind
        kFieldAccess_Command,
        // float value
        kFloatLiteral_Command,
        // Statement initializer, Expression test, Expression next, Statement body,
        // SymbolTable symbols
        kFor_Command,
        // Type type, uint16 function, uint8 argCount, Expression[] arguments
        kFunctionCall_Command,
        // uint16 declaration, Statement body, uint8 refCount
        kFunctionDefinition_Command,
        // uint16 id, Modifiers modifiers, String name, uint8 parameterCount, uint16[] parameterIds,
        // Type returnType
        kFunctionDeclaration_Command,
        // uint16 declaration
        kFunctionPrototype_Command,
        // bool isStatic, Expression test, Statement ifTrue, Statement ifFalse
        kIf_Command,
        // Expression base, Expression index
        kIndex_Command,
        // FunctionDeclaration function
        kInlineMarker_Command,
        // Variable* var, String typeName, String instanceName, uint8 sizeCount, Expression[] sizes
        kInterfaceBlock_Command,
        // int32 value
        kIntLiteral_Command,
        // int32 flags, int8 location, int16 offset, int16 binding, int8 index, int8 set,
        // int16 builtin, int8 inputAttachmentIndex
        kLayout_Command,
        // Layout layout, uint8 flags
        kModifiers8Bit_Command,
        // Layout layout, uint32 flags
        kModifiers_Command,
        kNop_Command,
        // uint8 op, Expression operand
        kPostfix_Command,
        // uint8 op, Expression operand
        kPrefix_Command,
        // uint8_t symbolTableCount, SymbolTable[] symbolTables, Elements elements,
        // bool useFlipRTUniform
        kProgram_Command,
        // Expression value
        kReturn_Command,
        // String name, Expression value
        kSetting_Command,
        // uint8_t parameterCount, Variable[] parameters, FunctionDeclaration decl,
        // FunctionDefinition defn
        kSharedFunction_Command,
        // uint16 id, Type structType
        kStructDefinition_Command,
        // uint16 id, String name, uint8 fieldCount, (Modifiers, String, Type)[] fields
        kStructType_Command,
        // bool isStatic, SymbolTable symbols, Expression value, uint8 caseCount,
        // (Expression value, uint8 statementCount, Statement[] statements)[] cases
        kSwitch_Command,
        // Expression base, uint8 componentCount, uint8[] components
        kSwizzle_Command,
        // uint16 id
        kSymbolRef_Command,
        // uint16 owned symbol count, Symbol[] ownedSymbols, uint16 symbol count,
        // (uint16/*index*/ | { uint16 kBuiltin_Symbol, String name })
        kSymbolTable_Command,
        // uint16 id, String name
        kSystemType_Command,
        // Expression test, Expression ifTrue, Expression ifFalse
        kTernary_Command,
        // uint16 id, FunctionDeclaration[] functions
        kUnresolvedFunction_Command,
        // uint16 id, Modifiers modifiers, String name, Type type, uint8 storage
        kVariable_Command,
        // uint16 varId, uint8 sizeCount, Expression[] sizes, Expression? value
        kVarDeclaration_Command,
        // Type baseType, uint8 varCount, VarDeclaration vars
        kVarDeclarations_Command,
        // uint16 varId, uint8 refKind
        kVariableReference_Command,
        kVoid_Command,
    };

    // src must remain in memory as long as the objects created from it do
    Rehydrator(Compiler& compiler, const uint8_t* src, size_t length,
            std::shared_ptr<SymbolTable> base = nullptr);

#ifdef SK_DEBUG
    ~Rehydrator();
#endif

    // Reads a symbol table and makes it current (inheriting from the previous current table)
    std::shared_ptr<SymbolTable> symbolTable();

    // Reads a collection of program elements and returns it
    std::vector<std::unique_ptr<ProgramElement>> elements();

    // Reads an entire program.
    std::unique_ptr<Program> program();

private:
    // If this ID appears in place of a symbol ID, it means the corresponding symbol isn't actually
    // present in the file as it's a builtin. The string name of the symbol follows.
    static constexpr uint16_t kBuiltin_Symbol = 65535;

    int8_t readS8() {
        SkASSERT(fIP < fEnd);
        return *(fIP++);
    }

    uint8_t readU8() {
        return this->readS8();
    }

    int16_t readS16() {
        uint8_t b1 = this->readU8();
        uint8_t b2 = this->readU8();
        return (b2 << 8) + b1;
    }

    uint16_t readU16() {
        return this->readS16();
    }

    int32_t readS32() {
        uint8_t b1 = this->readU8();
        uint8_t b2 = this->readU8();
        uint8_t b3 = this->readU8();
        uint8_t b4 = this->readU8();
        return (b4 << 24) + (b3 << 16) + (b2 << 8) + b1;
    }

    uint32_t readU32() {
        return this->readS32();
    }

    std::string_view readString() {
        uint16_t offset = this->readU16();
        uint8_t length = *(uint8_t*) (fStringStart + offset);
        const char* chars = (const char*) fStringStart + offset + 1;
        return std::string_view(chars, length);
    }

    void addSymbol(int id, const Symbol* symbol) {
        while ((size_t) id >= fSymbols.size()) {
            fSymbols.push_back(nullptr);
        }
        fSymbols[id] = symbol;
    }

    template<typename T>
    T* symbolRef(Symbol::Kind kind) {
        uint16_t result = this->readU16();
        SkASSERT(fSymbols.size() > result);
        return (T*) fSymbols[result];
    }

    /**
     * Reads either a symbol belonging to this program, or a named reference to a builtin symbol.
     * This has to be a separate method from symbolRef() because builtin symbols can be const, and
     * thus this method must have a const return, but there is at least one case in which we
     * specifically require a non-const return value.
     */
    const Symbol* possiblyBuiltinSymbolRef() {
        uint16_t id = this->readU16();
        if (id == kBuiltin_Symbol) {
            std::string_view name = this->readString();
            const Symbol* result = (*fSymbolTable)[name];
            SkASSERTF(result, "symbol '%s' not found", std::string(name).c_str());
            return result;
        }
        SkASSERT(fSymbols.size() > id);
        return fSymbols[id];
    }

    Layout layout();

    Modifiers modifiers();

    const Symbol* symbol();

    std::unique_ptr<ProgramElement> element();

    std::unique_ptr<Statement> statement();

    std::unique_ptr<Expression> expression();

    ExpressionArray expressionArray();

    const Type* type();

    ErrorReporter* errorReporter() { return fContext->fErrors; }

    ModifiersPool& modifiersPool() const { return *fContext->fModifiersPool; }

    Compiler& fCompiler;
    std::shared_ptr<Context> fContext;
    std::shared_ptr<SymbolTable> fSymbolTable;
    std::vector<const Symbol*> fSymbols;

    const uint8_t* fStringStart;
    const uint8_t* fIP;
    SkDEBUGCODE(const uint8_t* fEnd;)

    friend class AutoRehydratorSymbolTable;
    friend class Dehydrator;
};

}  // namespace SkSL

#endif
