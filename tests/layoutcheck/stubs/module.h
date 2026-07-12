#pragma once
// Qt-free stub of the Forge's modules/module.h, just enough to compile the
// module<name>.cpp layout classes for the parity check. See registertypes.h
// in the Forge for the authoritative type codes.
typedef char register_type_t;
#define REGISTER_INPUT     'I'
#define REGISTER_NORMALIZE 'N'
#define REGISTER_OUTPUT    'O'
#define REGISTER_GATE      'G'
#define REGISTER_BUTTON    'B'
#define REGISTER_LED       'L'
#define REGISTER_POT       'P'
#define REGISTER_ENCODER   'E'
#define REGISTER_SWITCH    'S'
#define REGISTER_RGB_LED   'R'
#define REGISTER_EXTRA     'X'
#define DATA_INDEX_G8_RGB_OFFSET 6

struct QPointF {
    float xv = 0, yv = 0;
    QPointF() = default;
    QPointF(float x, float y) : xv(x), yv(y) {}
    float x() const { return xv; }
    float y() const { return yv; }
};
struct QString {
    QString() = default;
    QString(const char*) {}
};
struct QVariant {
    int v = 0;
    int toInt() const { return v; }
};
struct AtomRegister {
    AtomRegister() = default;
    AtomRegister(register_type_t, unsigned, unsigned, unsigned) {}
};
class MainWindow;

class Module {
public:
    Module(MainWindow*, const QString&) {}
    virtual ~Module() {}
    virtual QString title() const = 0;
    virtual float hp() const = 0;
    virtual unsigned numRegisters(register_type_t) const { return 0; }
    virtual unsigned numberOffset(register_type_t) const { return 0; }
    virtual QPointF registerPosition(register_type_t, unsigned) const = 0;
    virtual float registerSize(register_type_t, unsigned) const = 0;
    virtual float labelDistance(register_type_t, unsigned) const { return 0; }
    virtual QPointF labelPosition(register_type_t, unsigned) const { return QPointF(0, 0); }
    virtual float labelWidth(register_type_t, unsigned) const { return 2.0; }
    virtual bool labelNeedsBackground(register_type_t, unsigned) const { return false; }
    virtual float rectAspect(register_type_t, unsigned) const { return 0.0; }
    virtual AtomRegister registerAtom(register_type_t, unsigned) const { return AtomRegister(); }
    QVariant data(int) const { return QVariant(); }
    unsigned g8Number() const { return 1; }
};
