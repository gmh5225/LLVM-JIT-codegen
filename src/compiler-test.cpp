#include <unordered_map>
#include "fake-gunit.h"
#include "ast.h"
#include "parser.h"
#include "dom-to-string.h"
#include "name-resolver.h"
#include "type-checker.h"

int64_t generate_and_execute(ltm::pin<ast::Ast> ast, bool dump_ir);  // defined in `generator.h/cpp`

namespace {

using std::unordered_map;
using std::string;
using std::move;
using ltm::pin;
using ltm::own;
using dom::Name;
using ast::Ast;

int64_t execute(const char* source_text, bool dump_all = false) {
    ast::initialize();
    auto ast = own<Ast>::make();
    auto start_module_name = ast->dom->names()->get("ak")->get("test");
    unordered_map<own<Name>, string> texts{ {start_module_name, source_text} };
    parse(ast, start_module_name, [&](pin<Name> name) {
        return move(texts[name]);
    });
    resolve_names(ast);
    check_types(ast);
    if (dump_all)
        std::cout << std::make_pair(ast.pinned(), ast->dom.pinned()) << "\n";
    return generate_and_execute(ast, dump_all);
}




TEST(Parser, Arrays) {
    ASSERT_EQ(42, execute(R"(
        class Node {
            x = 1;
            y = 0;
        }
        a = sys_Array;
        sys_Container_insert(a, 0, 10);
        a[0] := Node;
        a[1] := Node;
        a[1]&&_~Node?{
            _.x := 42;
            _.y := 33;
        };
        c = @a;
        sys_Array_delete(c, 0, 1);
        c[0]&&_~Node?_.x : -1
    )"));
}

TEST(Parser, BlobsAndIndexes) {
    ASSERT_EQ(42, execute(R"(
        b = sys_Blob;
        sys_Container_insert(b, 0, 3);
        b[1] := 42;
        c = @b;
        c[1]
    )"));
}

TEST(Parser, IntegerOps) {
    ASSERT_EQ(7, execute("(2 ^ 2 * 3 + 1) << (2-1) | (2+2) | (3 & (2>>1))"));
}

TEST(Parser, Doubles) {
    ASSERT_EQ(3, execute("int(3.14 - 0.2e-4 * 5.0)"));
}

TEST(Parser, Block) {
    ASSERT_EQ(3, execute("{1+1}+1"));
}

TEST(Parser, Functions) {
    ASSERT_EQ(9, execute(R"-(
      fn plus1(int a) int {
            a + 1
      }
      plus1(4) + plus1(3)
    )-"));
}

TEST(Parser, LambdaWithParams) {
    ASSERT_EQ(40, execute("(a){1+a}(3)*10"));
}

TEST(Parser, PassingLambdaToLambda) {
    ASSERT_EQ(99, execute(R"-(
      (a, b, xfn) {
            xfn((t) {
                a + b + t
            })
      } (2, 4, (vfn) {
            vfn(3) * vfn(5)
      })
    )-"));
}

TEST(Parser, Sequecnce) {
    ASSERT_EQ(44, execute("1; 3.14; 44"));
}

TEST(Parser, LocalAssignment) {
    ASSERT_EQ(10, execute("a = 2; a := a + 3; a * 2"));
}

TEST(Parser, MakeAndConsumeOptionals) {
    ASSERT_EQ(2, execute("(opt){ opt : 2 } (false ? 44)"));
}

TEST(Parser, MaybeChain) {
    ASSERT_EQ(3, execute(R"(
        a = +3.3;  // a: local of type optional(double), having value just(3.3)
        a ? int(_) : 0  // convert optional(double) to optional(int) and extract replacing `none` with 0
    )"));
}

TEST(Parser, IntLessThan) {
    ASSERT_EQ(3, execute("a = 2; a < 10 ? 3 : 44"));
}

TEST(Parser, IntNotEqual) {
    ASSERT_EQ(3, execute("a = 2; a != 10 ? 3 : 44"));
}

TEST(Parser, Loop) {
    ASSERT_EQ(39916800, execute(R"(
      a = 0;
      r = 1;
      loop {
          a := a + 1;
          r  := r * a;
          a > 10 ? r
      })"));
}

TEST(Parser, Classes) {
    ASSERT_EQ(3, execute(R"(
        class Point {
          x = 0;
          y = 2;
        }
        p=Point;
        p.x := 1;
        p.y + p.x
    )"));
}

TEST(Parser, ClassInstanceCopy) {
    ASSERT_EQ(4, execute(R"(
        class Point {
          x = 0;
          y = 2;
        }
        p=Point;
        p.x := 1;
        pb = @p;
        pb.x := 3;
        p.x + pb.x
    )"));
}

TEST(Parser, ClassMethods) {
    ASSERT_EQ(32, execute(R"(
        class Point {
          x = 1;
          y = 2;
          m() int { x+y }
        }
        class P3 {
          +Point{
            m() int { x+y+z }
          }
          z=3;
        }
        p=P3;
        p.x := 10;
        p.z := 20;
        p.m()
    )"));
}

TEST(Parser, Interfaces) {
    ASSERT_EQ(37, execute(R"(
        interface Movable {
          move(int x, int y);
        }
        class Point {
          x = 1;
          y = 2;
          m() int { x+y }
        }
        class P3 {
          +Point{
            m() int { x+y+z } 
          }
          +Movable {
            move(int dx, int dy) {
              x := x + dx;
              y := y + dy
            }
          }
          z = 3;
        }
        p=P3;         // 1, 2, 3
        p.x := 10;    // 10, 2, 3
        p.z := 20;    // 10, 2, 20
        p.move(2, 3); // 12, 5, 20
        p.m()         // 37
    )"));
}

TEST(Parser, TwoInterfaces) {
    ASSERT_EQ(201, execute(R"(
        interface Movable {
          moveTo(int x, int y);
        }
        interface Sizeable {
          resize(int width, int height);
        }
        class Point {
          x = 0;
          y = 0;
        }
        class Widget {
          +Point;
          +Movable {
            moveTo(int x, int y) {
              this.x := x;
              this.y := y;
            }
          }
          +Sizeable {
            resize(int width, int height) {
              size.x := width;
              size.y := height;
            }
          }
          size = Point;
        }
        p = Widget;
        p.moveTo(1,2);
        p.resize(100,200);
        p.x + p.size.y
    )"));
}

TEST(Parser, PromisedCast) {
    ASSERT_EQ(24, execute(R"(
        interface Movable {
          moveTo(int x, int y);
        }
        class Point {
          +Movable {
            moveTo(int x, int y) {
              this.x := x;
              this.y := y;
            }
          }
          x = 0;
          y = 0;
        }
        class Widget {
          +Movable {
            moveTo(int x, int y) {
              pos.moveTo(x, y);
            }
          }
          pos = Point;
        }
        p = Widget~Movable;
        p.moveTo(1, 2);
        p := Point;
        p.moveTo(10, 20);
        24
    )"));
}

TEST(Parser, ClassCast) {
    ASSERT_EQ(24, execute(R"(
        class Point {
          x = 0;
          y = 0;
        }
        class Widget {
          +Point;
          color = 24;
        }
        p = Widget~Point;  // `p` is a `Point` holder initialized with a `Widget` instance
        p~Widget?_.color : 0     // cast `p` to `Widget` and return its `color` field on success
    )"));
}

TEST(Parser, InterfaceCast) { // TODO interface dispatch with collisions
    ASSERT_EQ(47, execute(R"(
        interface Opaque {
          bgColor() int;
        }
        class Point {
          x = 0;
          y = ~0;
        }
        class Widget {
          +Point;
          +Opaque { bgColor() int { color } }
          color = 7;
        }
        p = Point;
        w = Widget~Point;
        a = p~Opaque?_.bgColor() : 40;  // expected to fallback to 40
        b = w~Opaque?_.bgColor() : 50;  // expected to return 7
        a+b
    )"));
}

TEST(Parser, Weak) {
    ASSERT_EQ(22, execute(R"(
        class Point {
          x = 0;
        }
        p = Point;
        w = &p;
        p.x := 22;
        r = w?_.x : 100;
        p := Point;
        w ? r := r + _.x;
        r
    )"));
}

TEST(Parser, TopoCopy) {
    ASSERT_EQ(35, execute(R"(
        class Node {
          parent = &Node;  // Weak(Node) = null
          left = ?Node;    // Optional(own(Node)) = null
          right = ?Node;

          scan(&Node expectedParent) int {
            lcount = left?_.scan(&this) : 0;
            rcount = right?_.scan(&this) : 0;
            this.parent == expectedParent
                ? lcount + rcount + 1
                : -100
          }
        }
        root = Node;
        root.left := +Node;
        root.right := +Node;
        root.left?_.parent := &root;
        root.right?_.parent := &root;

        oldSize = root.scan(&Node);

        root.left := +@root;
        root.left?_.parent := &root;

        oldSize * 10 + root.scan(&Node)
    )"));
}

TEST(Parser, ForeignFunctionCall) {
    ASSERT_EQ(42, execute(R"(
        fn sys_foreignTestFunction(int x) int;
        sys_foreignTestFunction(4*10);
        sys_foreignTestFunction(2)
    )"));
}

TEST(Parser, LogicalOr) {
    ASSERT_EQ(42, execute(R"(
        a = 3;
        (a > 2 || a < 4) ? 42 : 0
    )"));
}

TEST(Parser, LogicalAnd) {
    ASSERT_EQ(42, execute(R"(
        a = 3;
        a > 2 && a < 4 ? 42 : 0
    )"));
}

TEST(Parser, Raii) {
    ASSERT_EQ(2, execute(R"(
        class Font {
            ttfHandle = 0;
            setId(int id) {
                ttfHandle != 0 ? sys_foreignTestFunction(-ttfHandle);
                ttfHandle := id;
                id != 0 ? sys_foreignTestFunction(id);
            }
        }
        fn Font_dispose(Font f) {
            f.ttfHandle != 0 ? sys_foreignTestFunction(-f.ttfHandle);
        }
        fn Font_afterCopy(Font f) {
            f.ttfHandle != 0 ? sys_foreignTestFunction(f.ttfHandle);
        }
        fn sys_foreignTestFunction(int x) int;

        {
            sys_foreignTestFunction(2);
            fa = Font;
            fa.setId(42);   // sys_foreignTestFunction_state= 2+42
            fb = @fa;       // sys_foreignTestFunction_state= 2+42+42
        };                  // sys_foreignTestFunction_state= 2+42+42-42-42 = 2
        sys_foreignTestFunction(0)
    )"));
}

}  // namespace
