// Bindings
#include "CPyCppyy.h"
#include "CPPConstructor.h"
#include "CPPInstance.h"
#include "Executors.h"
#include "MemoryRegulator.h"

// Standard
#include <string>


//- protected members --------------------------------------------------------
bool CPyCppyy::CPPConstructor::InitExecutor_(Executor*& executor, CallContext*)
{
// pick up special case new object executor
    executor = CreateExecutor("__init__");
    return true;
}

//- public members -----------------------------------------------------------
PyObject* CPyCppyy::CPPConstructor::GetDocString()
{
// GetMethod() may return an empty function if this is just a special case place holder
    const std::string& clName = Cppyy::GetFinalName(this->GetScope());
    return CPyCppyy_PyUnicode_FromFormat("%s::%s%s",
        clName.c_str(), clName.c_str(), this->GetMethod() ? this->GetSignatureString().c_str() : "()");
}

//----------------------------------------------------------------------------
PyObject* CPyCppyy::CPPConstructor::Call(
        CPPInstance*& self, PyObject* args, PyObject* kwds, CallContext* ctxt)
{
// preliminary check in case keywords are accidently used (they are ignored otherwise)
    if (kwds && PyDict_Size(kwds)) {
        PyErr_SetString(PyExc_TypeError, "keyword arguments are not yet supported");
        return nullptr;
    }

// setup as necessary
    if (!this->Initialize(ctxt))
        return nullptr;                     // important: 0, not Py_None

// fetch self, verify, and put the arguments in usable order
    if (!(args = this->PreProcessArgs(self, args, kwds)))
        return nullptr;

// translate the arguments
    if (!this->ConvertAndSetArgs(args, ctxt)) {
        Py_DECREF(args);
        return nullptr;
    }

// perform the call, 0 makes the other side allocate the memory
    Long_t address = (Long_t)this->Execute(nullptr, 0, ctxt);

// done with filtered args
    Py_DECREF(args);

// return object if successful, lament if not
    if (address) {
        Py_INCREF(self);

    // note: constructors are no longer set to take ownership by default; instead that is
    // decided by the method proxy (which carries a creator flag) upon return
        self->Set((void*)address);

    // TODO: consistent up or down cast ...
        MemoryRegulator::RegisterPyObject(self, (Cppyy::TCppObject_t)address);

   // done with self
        Py_DECREF(self);

        Py_RETURN_NONE;                     // by definition
    }

    if (!PyErr_Occurred())   // should be set, otherwise write a generic error msg
        PyErr_SetString(PyExc_TypeError, const_cast<char*>(
            (Cppyy::GetScopedFinalName(GetScope()) + " constructor failed").c_str()));

// do not throw an exception, '0' might trigger the overload handler to choose a
// different constructor, which if all fails will throw an exception
    return nullptr;
}


//----------------------------------------------------------------------------
PyObject* CPyCppyy::CPPAbstractClassConstructor::Call(
        CPPInstance*&, PyObject*, PyObject*, CallContext*)
{
// do not allow instantiation of abstract classes
    PyErr_Format(PyExc_TypeError, "cannot instantiate abstract class \'%s\'",
        Cppyy::GetScopedFinalName(this->GetScope()).c_str());
    return nullptr;
}

//----------------------------------------------------------------------------
PyObject* CPyCppyy::CPPNamespaceConstructor::Call(
        CPPInstance*&, PyObject*, PyObject*, CallContext*)
{
// do not allow instantiation of namespaces
    PyErr_Format(PyExc_TypeError, "cannot instantiate namespace \'%s\'",
        Cppyy::GetScopedFinalName(this->GetScope()).c_str());
    return nullptr;
}


//----------------------------------------------------------------------------
PyObject* CPyCppyy::CPPDispatcherConstructor::PreProcessArgs(
    CPPInstance*& self, PyObject* args, PyObject* kwds)
{
// normal processing to find self
    args = this->CPPMethod::PreProcessArgs(self, args, kwds);

// add self again as an additional first argument to make sure it propagates
// to the C++-side
    Py_ssize_t sz = PyTuple_GET_SIZE(args);
    PyObject* newArgs = PyTuple_New(sz+1);
    for (int i = 0; i < sz; ++i) {
        PyObject* item = PyTuple_GET_ITEM(args, i);
        Py_INCREF(item);
        PyTuple_SET_ITEM(newArgs, i+1, item);
    }

    Py_INCREF(self);
    PyTuple_SET_ITEM(newArgs, 0, (PyObject*)self);

    Py_DECREF(args); // which was a new args from the base method
    return newArgs;
}
