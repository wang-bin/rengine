
#pragma once

#include <assert.h>
#include <fstream>
#include "objectmodel.h"

class CodeGenerator
{
public:
    void setClasses(const std::map<std::string, Class *> &classes) { m_classes = classes; }

    void generate();
    void setIncludeMain(bool include) { m_includeMain = include; }

private:
    bool generateClass(Class *clazz);

    void writeDisclaimer(std::ostream &stream);
    void writeIncludes(std::ostream &stream, Class *clazz);
    void writeBeginningOfClass(std::ostream &stream, Class *clazz);
    void writeEndOfClass(std::ostream &stream, Class *clazz);
    void writeFunctions(std::ostream &stream, Class *clazz);
    void writeProperties(std::ostream &stream, Class *clazz);
    void writePropertyMemberVars(std::ostream &ostream, Class *clazz);
    void writeSignals(std::ostream &stream, Class *clazz);
    void writeResources(std::ostream &stream, Class *clazz);
    void writeObjects(std::ostream &stream, Class *clazz);
    void writeObject(std::ostream &stream, Object *object);
    void writeObjectMemberVars(std::ostream &stream, Object *object);
    void writeSignalHandlers(std::ostream &stream, Object *object, Class *clazz);
    void writeReplicators(std::ostream &stream, Class *clazz);
    void writeDummyMain(std::ostream &stream, Class *clazz);

    std::map<std::string, Class *> m_classes;

    std::string m_outputDir;

    bool m_includeMain = false;
};

void CodeGenerator::generate()
{
    bool atLeastOne = false;
    for (auto i : m_classes) {
        if (!i.second->declarationOnly) {
            atLeastOne = true;
            generateClass(i.second);
        }
    }
    if (!atLeastOne) {
        std::cerr << "No class definitions present in input. Nothing generated" << std::endl;
        return;
    }
}

bool CodeGenerator::generateClass(Class *clazz)
{
    std::string dir = m_outputDir.empty() ? "." : m_outputDir;
    std::string fileName = dir + "/generated_" + clazz->name + ".h";
    std::ofstream stream(fileName, std::ofstream::out);
    if (!stream.is_open()) {
        std::cerr << "Failed to open file='" << fileName << "'" << std::endl;
        return false;
    }

    writeDisclaimer(stream);
    stream << "#pragma once" << std::endl << std::endl;
    writeIncludes(stream, clazz);
    writeBeginningOfClass(stream, clazz);
    writeFunctions(stream, clazz);
    writeProperties(stream, clazz);
    writeSignals(stream, clazz);
    writeResources(stream, clazz);
    writePropertyMemberVars(stream, clazz);
    writeObjectMemberVars(stream, clazz->root);
    writeSignalHandlers(stream, clazz->root, clazz);
    writeObjects(stream, clazz);

    if (!clazz->replicators.empty())
        writeReplicators(stream, clazz);

    writeEndOfClass(stream, clazz);
    stream.close();

    std::cerr << "wrote: " << fileName << std::endl;

    if (m_includeMain) {
        std::string mainFileName = dir + "/main_" + clazz->name + ".cpp";
        std::ofstream mainStream(mainFileName, std::ofstream::out);
        if (!mainStream.is_open()) {
            std::cerr << "Failed to open file='" << mainFileName << "'" << std::endl;
        }
        writeDummyMain(mainStream, clazz);
        mainStream.close();
        std::cerr << "wrote: " << mainFileName << std::endl;
    }


    return true;
}

void CodeGenerator::writeDisclaimer(std::ostream &s)
{
    s << "/*" << std::endl
      << " * This file is generated!" << std::endl
      << " * Any changes you make to it will be lost when it is regenerated." << std::endl
      << " */" << std::endl << std::endl;
}

void CodeGenerator::writeIncludes(std::ostream &s, Class *clazz)
{
    std::set<std::string> includes;

    // First add all imported classes' includes.
    for (auto i : m_classes) {
        const Class *c = i.second;
        if (!c->include.empty())
            includes.insert(i.second->include);
    }
    // Then include all includes to classes we're replecating..
    // ### We should be checking for cyclic deps here..
    for (auto replicator : clazz->replicators) {
        auto classIt = m_classes.find(replicator.clazz);
        if (classIt != m_classes.end()) {
            Class *c = classIt->second;
            if (!c->declarationOnly) {
                // a generated class,
                includes.insert("generated_" + c->name + ".h");
            } else if (!c->include.empty()) {
                includes.insert(c->include);
            }
        }
    }

    for (auto i : includes)
        s << "#include \"" << i << "\"" << std::endl;
    s << std::endl;
}

void CodeGenerator::writeBeginningOfClass(std::ostream &s, Class *clazz)
{
    s << "class " << clazz->name << " : public rengine::SignalEmitter" << std::endl
      << "{" << std::endl
      << "public:" << std::endl
      << "    bool initialized = false;" << std::endl
      << "    // initialize function.." << std::endl
      << "    void initialize(rengine::ResourceManager *manager)" << std::endl
      << "    {" << std::endl
      << "        assert(!initialized);" << std::endl
      << "        initResources(manager);" << std::endl
      << "        initObjects();" << std::endl;
    if (!clazz->replicators.empty())
        s << "        initReplicators(manager);" << std::endl;
    s << "        initialized = true;" << std::endl
      << "    }" << std::endl
      << std::endl;
}

void CodeGenerator::writeEndOfClass(std::ostream &s, Class *clazz)
{
    s << "}; // end of class " << clazz->name << std::endl << std::endl;
}

void CodeGenerator::writeFunctions(std::ostream &s, Class *clazz)
{
    s << "    // functions forward declarations (to be implemented by user)" << std::endl;
    for (auto i : clazz->functions) {
        s << "    " << i.signature << ";" << std::endl;
    }
    s << std::endl;
}

void CodeGenerator::writeSignals(std::ostream &s, Class *clazz)
{
    s << "    // signals" << std::endl;
    for (auto i : clazz->signals)
        s << "    static rengine::Signal<" << i.signature << "> " << i.name << ";" << std::endl;
    s << std::endl;
}

void CodeGenerator::writeProperties(std::ostream &s, Class *clazz)
{
    s << "    // properties" << std::endl;
    for (auto i : clazz->properties) {
        std::string name = i.name;
        name[0] = std::toupper(name[0]);
        s << "    static rengine::Signal<> on" << name << "Changed;" << std::endl
          << "    const " << i.type << " &" << i.name << "() const {" << std::endl
          << "        return m_" << i.name << ";" << std::endl
          << "    }" << std::endl
          << "    void set" << name << "(const " << i.type << " &" << i.name << ") {" << std::endl
          << "        if (m_" << i.name << " == " << i.name << ")" << std::endl
          << "            return;" << std::endl
          << "        m_" << i.name << " = " << i.name << ";" << std::endl
          << "        on" << name << "Changed.emit(this);" << std::endl
          << "    }" << std::endl
          << std::endl;
    }
}

void CodeGenerator::writeResources(std::ostream &s, Class *clazz)
{
    s << "    // resources" << std::endl;
    for (auto i : clazz->resources) {
        s << "    " << i.type << " *" << i.name << " = nullptr;" << std::endl;
    }
    s << std::endl
      << "    void initResources(rengine::ResourceManager *manager) {" << std::endl;
    for (auto i : clazz->resources) {
        s << "        " << i.name << " = manager->acquire<" << i.type << ">(\"" << i.initializer << "\");" << std::endl;
    }
    s << "    }" << std::endl
      << std::endl;
}

inline std::string cg_default_value_for_type(const std::string &type)
{
    if (type == "bool") return "false";
    else if (type == "boolean") return "false";
    else if (type == "char") return "0";
    else if (type == "unsigned char") return "0";
    else if (type == "short") return "0";
    else if (type == "unsigned short") return "0";
    else if (type == "int") return "0";
    else if (type == "unsigned int") return "0";
    else if (type == "unsigned") return "0";
    else if (type == "long") return "0";
    else if (type == "unsigned long") return "0";
    else if (type == "long long") return "0";
    else if (type == "unsigned long long") return "0";
    else if (type == "float") return "0.0f";
    else if (type == "double") return "0.0";
    else if (type.find("*") != std::string::npos) return "nullptr";
    return "";
}

void CodeGenerator::writePropertyMemberVars(std::ostream &s, Class *clazz)
{
    s << "    // property member vars" << std::endl;
    for (auto i : clazz->properties) {
        s << "    " << i.type << " m_" << i.name;
        std::string init = cg_default_value_for_type(i.type);
        if (!init.empty())
            s << " = " << init;
        s << ";" << std::endl;
    }
    s << std::endl;
}

void CodeGenerator::writeObjects(std::ostream &s, Class *clazz)
{
    s << "    // The objects.." << std::endl
      << "    void initObjects() {" << std::endl;
    writeObject(s, clazz->root);
    s << "    }" << std::endl
      << std::endl;
}

void CodeGenerator::writeObjectMemberVars(std::ostream &s, Object *object)
{
    // the root (first) object
    if (!object->parent)
        s << "    // object member variables..." << std::endl;

    s << "    " << object->clazz->name << " *" << object->id << " = nullptr;" << std::endl;
    for (auto i : object->children)
        writeObjectMemberVars(s, i);

    if (!object->parent)
        s << std::endl;
}

inline std::string cg_binding_name(const std::string &id, const std::string &property)
{
    return "binding_" + id + "_" + property;
}

void CodeGenerator::writeObject(std::ostream &s, Object *object)
{
    if (object->parent)
        s << std::endl;
    s << "        " << object->id << " = " << object->clazz->alloc << ";" << std::endl;
    for (auto i : object->propertyValues) {
        std::string name = i.first;
        name[0] = std::toupper(name[0]);
        const Value &value = i.second;
        if (value.type == Value::StringValue) {
            s << "        " << object->id << "->set" << name << "(" << value.stringValue << ");" << std::endl;
        } else if (value.type == Value::NumberValue) {
            s << "        " << object->id << "->set" << name << "(" << value.numberValue << ");" << std::endl;
        } else if (value.type == Value::BindingValue) {
            for (const Binding::Dependency &dep : value.bindingValue->dependencies) {
                std::string property = dep.propertyName;
                property[0] = std::toupper(property[0]);
                s << "        " << dep.objectId << "->on" << property << "Changed.connect("
                  << dep.objectId << ", &"
                  << cg_binding_name(object->id, i.first) << ");" << std::endl;
            }
            s << "        " << cg_binding_name(object->id, i.first) << ".self = this;" << std::endl;
        }
    }
    if (object->parent)
        s << "        " << object->parent->id << "->append(" << object->id << ");" << std::endl;
    for (auto i : object->children)
        writeObject(s, i);
}

void CodeGenerator::writeSignalHandlers(std::ostream &s, Object *object, Class *clazz)
{
    // the root...
    if (!object->parent)
        s << "    // Signal handlers..." << std::endl;
    for (auto i : object->propertyValues) {
        std::string propertyName = i.first;
        const Value &value = i.second;
        if (value.type == Value::BindingValue) {
            propertyName[0] = std::toupper(propertyName[0]);
            std::string bindexpr = "expression_" + cg_binding_name(object->id, i.first);
            s << "    void " << bindexpr << "() { " << std::endl
              << "        " << object->id << "->set" << propertyName << "(" << value.bindingValue->expression << ");" << std::endl
              << "    }" << std::endl
              << "    struct : public rengine::SignalHandler<> {" << std::endl
              << "        " << clazz->name << " *self = nullptr;" << std::endl
              << "        void onSignal() override { self->" << bindexpr << "(); }" << std::endl
              << "    } " << cg_binding_name(object->id, i.first) << ";" << std::endl;
        }
    }
    for (auto i : object->children)
        writeSignalHandlers(s, i, clazz);

    if (!object->parent)
        s << std::endl;
}

void CodeGenerator::writeDummyMain(std::ostream &s, Class *clazz)
{
    s << "#include \"generated_" << clazz->name << ".h\"" << std::endl
      << std::endl
      << "RENGINE_MAIN(rengine::SurfaceInterfaceForGenerated<" << clazz->name << ">);" << std::endl;
}

void CodeGenerator::writeReplicators(std::ostream &s, Class *clazz)
{
    s << "    // replicators" << std::endl;

    for (const Replicator &r : clazz->replicators)
        s << "    std::vector<" << r.clazz << "> replicator_" << r.id << ";" << std::endl;

    s << "    void initReplicators(rengine::ResourceManager *manager) {" << std::endl
      << "        int count;" << std::endl;
    for (const Replicator &r : clazz->replicators) {
        assert(m_classes.find(r.clazz) != m_classes.end());
        Class *instanceClass = m_classes.find(r.clazz)->second;
        s << "        count = " << r.count.numberValue << ";" << std::endl
          << "        for (int index=0; i<count; ++i) {" << std::endl
          // ### TODO also handle declaratationOnly classes
          << "            " << instanceClass->name << " *instance = new " << instanceClass->name << "()" << std::endl
          << "            instance->initialize(manager);" << std::endl
          << "            replicator_" << r.id << ".push_back(instance);" << std::endl
          << "            " << r.initializor << std::endl
          << "            " << r.parentId << "->append(instance->root);" << std::endl
          << "        }" << std::endl;
    }
    s << std::endl;
}