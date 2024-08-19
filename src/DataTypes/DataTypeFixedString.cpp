#include <Columns/ColumnFixedString.h>

#include <DataTypes/DataTypeFixedString.h>
#include <DataTypes/DataTypeFactory.h>
#include <DataTypes/Serializations/SerializationFixedString.h>

#include <Parsers/IAST.h>
#include <Parsers/ASTLiteral.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int NUMBER_OF_ARGUMENTS_DOESNT_MATCH;
    extern const int UNEXPECTED_AST_STRUCTURE;
}


std::string DataTypeFixedString::doGetName() const
{
    return "FixedString(" + toString(n) + ")";
}

MutableColumnPtr DataTypeFixedString::createColumn() const
{
    return ColumnFixedString::create(n);
}

Field DataTypeFixedString::getDefault() const
{
    return String();
}

bool DataTypeFixedString::equals(const IDataType & rhs) const
{
    return typeid(rhs) == typeid(*this) && n == static_cast<const DataTypeFixedString &>(rhs).n;
}

SerializationPtr DataTypeFixedString::doGetDefaultSerialization() const
{
    return std::make_shared<SerializationFixedString>(n);
}


static DataTypePtr create(const ASTPtr & arguments)
{
    if (!arguments || arguments->children.size() != 1)
        throw Exception(ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH,
                        "FixedString data type family must have exactly one argument - size in bytes");

    const auto * argument = arguments->children[0]->as<ASTLiteral>();
    if (!argument || argument->value.getType() != Field::Types::UInt64 || argument->value.safeGet<UInt64>() == 0)
        throw Exception(ErrorCodes::UNEXPECTED_AST_STRUCTURE,
                        "FixedString data type family must have a number (positive integer) as its argument");

    return std::make_shared<DataTypeFixedString>(argument->value.safeGet<UInt64>());
}


void registerDataTypeFixedString(DataTypeFactory & factory)
{
    factory.registerDataType("FixedString", create);

    /// Compatibility alias.
    factory.registerAlias("BINARY", "FixedString", DataTypeFactory::Case::Insensitive);
}

}
