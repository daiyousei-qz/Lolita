## Project Lolita Revisited

Using Project Lolita:

```
[Configuration File] -> [Parser Information] -> [Ast Model] -> [Parser Information] -> [Parser]
```

Configuration File:

```
Top-level entity: enum-decl, base-decl, klass-decl, token-decl, rule-decl
By convention, last rule is root of the syntax tree

rule Name : VariableType
    = [SymbolName Fold-Op]... [Arrow-Op]
    = ...
    ;
    
Operations:
- !               | select the node and return
- &               | push the item into returned vector
- :name           | assign the item to the specific member of returned object
- -> EnumName     | create an enum node and return
- -> TypeName     | create a new object of node the type specified and return
- -> _            | create a new object node of the same type with variable and return
```



Example Usage:

```
// ast model generation
std::cout << BootstrapParser(configPath);

// parser
auto parser = CreateParser(loli::ExampleParsingEnvironment);
auto result = parser->Parse(text);
```



```
[Parser Information]
- Token Definition
- Type Definition(Interfaces, Classes, Enums)
- Grammar Definition
```



```c++
template <typename ...Ts>
class AstNodeBundle
{
  int offset;
  // Ts values_...;
}


class SomeAstNode : AstNodeBundle<Ts...>
{
  const auto& SomeItemA() const { return GetItem<0>(); }
  const auto& SomeItemB() const { return GetItem<1>(); }
}
```

Ast model:

```
AstNode(offset):
- Token
- Enum
- Object(Base or Klass)

Two enhancing type: Vecter and Optional

AstItem is either of
- AstToken
- AstEnum
- AstObject*
- AstVector*
- AstOptional

NOTE an AstItem could be stored in AstItemWrapper
```

Generated Parser Sample:

```c++
// Forward Declaration
// ...

// Enums
// ...

// Bases
class SomeBase : public AstObject
{
  struct Visitor { /* ... */ }
  
  virtual void Accept(Visitor&) = 0;
};

// Klasses
class SomeKlass : public SomeBase, private DataBundle</* Ts... */>
{
public:
  friend class BasicAstTypeProxy<SomeKlass>;

  const auto& SomeMemberA() const { return GetItem<0>(); }
  const auto& SomeMemberB() const { return GetItem<1>(); }

  void Accept(SomeBase::Visitor& v) override { v.Visit(*this); }
};

// Archive
ParserArchive& GetParserArchive()
{
  static auto archive = [](){
    // ...
  }();
  
  return archive;
}
```

