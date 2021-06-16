#ifndef SDF_H
#define SDF_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>
#include <cmath>
#include <regex>
#include <typeinfo>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <libyang/libyang.h>

#include <dirent.h>

enum jsonDataType
{
    json_number,
    json_string,
    json_boolean,
    json_integer,
    json_array,
    json_object,
    json_type_undef
};
enum jsonSchemaFormat
{
    json_date_time,
    json_date,
    json_time,
    json_uri,
    json_uri_reference,
    json_uuid,
    json_format_undef
};
enum sdfSubtype
{
    sdf_byte_string,
    sdf_unix_time,
    sdf_subtype_undef
};
enum refOrReq {REF, REQ};

class sdfCommon;
class sdfThing;
class sdfObject;
class sdfProperty;
class sdfData;
class sdfFile;

std::string jsonDTypeToString(jsonDataType type);
jsonDataType stringToJsonDType(std::string str);
sdfCommon* refToCommon(std::string ref, std::string nsPrefix = "");
std::vector<std::tuple<std::string, sdfCommon*>> assignRefs(
        std::vector<std::tuple<std::string, sdfCommon*>> unassignedRefs);
std::string  correctValue(std::string val);
bool validateJson(nlohmann::json sdf,
        std::string schemaFileName = "sdf-validation.cddl");
bool validateFile(std::string fileName,
        std::string schemaFileName = "sdf-validation.cddl");

#define INDENT_WIDTH 2

// sdfObject, sdfProperty, sdfAction, sdfEvent and sdfData are sdfCommons
class sdfCommon
{
public:
    sdfCommon(std::string _name = "", std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {}, sdfFile *_file = NULL);
    virtual ~sdfCommon();
    // getters
    std::string getName() const;
    const char* getNameAsArray() const;
    std::string getDescription();
    const char* getDescriptionAsArray();
    std::string getLabel();
    const char* getLabelAsArray();
    sdfCommon* getReference();
    std::vector<sdfCommon*> getRequired();
    //sdfCommon* getParentCommon();
    sdfData* getSdfDataReference() const;
    sdfData* getSdfPropertyReference() const;
    sdfData* getThisAsSdfData();
    sdfFile* getParentFile() const;
    sdfFile* getTopLevelFile();
    virtual sdfCommon* getParent() const = 0;
    //virtual bool hasChild(sdfCommon *child) const = 0;
    std::string getDefaultNamespace();
    std::map<std::string, std::string> getNamespaces();
    // setters
    void setName(std::string _name);
    void setLabel(std::string _label);
    void setDescription(std::string dsc);
    void addRequired(sdfCommon *common);
    void setReference(sdfCommon *common);
    void setParentFile(sdfFile *file);
    //void setParentCommon(sdfCommon *parentCommon);
    // printing
    std::string generateReferenceString(bool import = false);
    virtual std::string generateReferenceString(sdfCommon *child,
            bool import = false) = 0;
    nlohmann::json commonToJson(nlohmann::json prefix);
    void jsonToCommon(nlohmann::json input);
private:
    std::string name;
    std::string description;
    std::string label;
    sdfCommon *reference;
    std::vector<sdfCommon*> required;
    //sdfCommon *parent;
    sdfFile *parentFile;
};

class sdfObjectElement : virtual public sdfCommon
{
public:
    sdfObjectElement(std::string _name = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL);
    ~sdfObjectElement();
    sdfObject* getParentObject() const;
    sdfCommon* getParent() const;
    void setParentObject(sdfObject *parentObject);
    virtual std::string generateReferenceString(sdfCommon *child,
            bool import = false) = 0;
    //virtual bool hasChild(sdfCommon *child) const = 0;
private:
    sdfObject *parentObject;
};

class sdfInfoBlock
{
public:
    sdfInfoBlock(std::string _title = "", std::string _version = "",
                std::string _copyright = "", std::string _license= "");
    // getters
    std::string getTitle();
    std::string getVersion();
    std::string getCopyright();
    std::string getLicense();
    const char* getTitleAsArray() const;
    const char* getVersionAsArray() const;
    const char* getCopyrightAsArray() const;
    const char* getLicenseAsArray() const;
    // parsing
    nlohmann::json infoToJson(nlohmann::json prefix);
    sdfInfoBlock* jsonToInfo(nlohmann::json input);
private:
    std::string title;
    std::string version;
    std::string copyright;
    std::string license;
};

class sdfNamespaceSection
{
public:
    sdfNamespaceSection();
    sdfNamespaceSection(std::map<std::string, std::string> _namespaces,
                        std::string _default_ns);
    //~sdfNamespaceSection();
    // getters
    std::map<std::string, std::string> getNamespaces();
    std::map<const char*, const char*> getNamespacesAsArrays();
    std::string getDefaultNamespace();
    const char* getDefaultNamespaceAsArray();
    std::string getNamespaceString() const;
    std::map<std::string, sdfFile*> getNamedFiles() const;
    // setters
    void removeNamespace(std::string pre);
    void addNamespace(std::string pre, std::string ns);
    void updateNamedFiles();
    // parsing
    nlohmann::json namespaceToJson(nlohmann::json prefix);
    sdfNamespaceSection* jsonToNamespace(nlohmann::json input);
    void makeDefinitionsGlobal();
private:
    // map of namespace prefixes/short names to namespace URIs
    std::map<std::string, std::string> namespaces;
    std::string default_ns;
    // namedFiles maps (default) namespaces to files
    std::map<std::string, sdfFile*> namedFiles;
};

class sdfData : virtual public sdfCommon
{
public:
    sdfData();
    sdfData(std::string _name,
            std::string _description,
            std::string _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL,
            std::vector<sdfData*> _choice = {});
    sdfData(std::string _name,
            std::string _description,
            jsonDataType _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL,
            std::vector<sdfData*> _choice = {});
    sdfData(sdfData &data);
    sdfData(sdfProperty &prop);
    ~sdfData();
    // Setters for member variables (different for types)
    // check which of the sdfData data qualities are designated for which
    // data type (number, string, ...)
    void setNumberData(float _constant = NAN,
            float _default = NAN,
            float _min = NAN,
            float _max = NAN,
            float _multipleOf = NAN);
    // TODO: constant and default should not be set at the same time?
    void setStringData(std::string _constant = "",
                       std::string _default = "",
                       float _minLength = NAN,
                       float _maxLength = NAN,
                       std::string _pattern = "",
                       jsonSchemaFormat _format = json_format_undef,
                       std::vector<std::string> _enum = {});
    void setBoolData(bool _constant = false,
                     bool defineConst = false,
                     bool _default = false,
                     bool defineDefault = false);
    void setIntData(int _constant = -1,
                    bool defineConst = false,
                    int _default = -1,
                    bool defineDefault = false,
                    float _min = NAN,
                    float _max = NAN);
    void setArrayData(float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                      sdfData *_itemConstr = NULL);
    // other setters
    void setSimpType(jsonDataType _type);
    void setType(std::string _type);
    void setType(jsonDataType _type);
    void setDerType(std::string _type);
    void setUnits(std::string _units, float _scaleMin = NAN,
            float _scaleMax = NAN);
    void setReadWrite(bool _readable, bool _writable);
    void setReadable(bool _readable);
    void setWritable(bool _writable);
    void setObserveNull(bool _observable, bool _nullable);
    void setFormat(jsonSchemaFormat _format);
    void setSubtype(sdfSubtype _subtype);
    void setUniqueItems(bool unique);
    void setMinimum(double min);
    void setMaximum(double max);
    void setMinInt(int64_t min);
    void setMaxInt(uint64_t max);
    void eraseMinInt();
    void eraseMaxInt();
    void setMultipleOf(float mult);
    void setMaxItems(float maxItems);
    void setMinItems(float minItems);
    void setMinLength(float min);
    void setMaxLength(float max);
    void setPattern(std::string pattern);
    void setConstantBool(bool constantBool);
    void setConstantInt(int64_t constantInt);
    void setConstantNumber(float constantNumber);
    void setConstantString(std::string constantString);
    void setDefaultBool(bool defaultBool);
    void setDefaultInt(int64_t defaultInt);
    void setDefaultNumber(float defaultNumber);
    void setDefaultString(std::string defaultString);
    void setConstantArray(std::vector<bool> constantArray);
    void setDefaultArray(std::vector<bool> defaultArray);
    void setConstantArray(std::vector<int64_t> constantArray);
    void setDefaultArray(std::vector<int64_t> defaultArray);
    void setConstantArray(std::vector<float> constantArray);
    void setDefaultArray(std::vector<float> defaultArray);
    void setConstantArray(std::vector<std::string> constantArray);
    void setDefaultArray(std::vector<std::string> defaultArray);
    void setConstantObject(sdfData *object);
    void setDefaultObject(sdfData *object);
    void setEnumString(std::vector<std::string> enm);
    // Todo: constant and default needed for object-type? No?
    //void setConstantArray(std::vector<sdfData*> constantArray);
    //void setDefaultArray(std::vector<sdfData*> defaultArray);
    void setItemConstr(sdfData* constr);
    void setParentCommon(sdfCommon *parentCommon);
    void setChoice(std::vector<sdfData*> choices);
    void addChoice(sdfData *choice);
    void addObjectProperty(sdfData *property);
    void setObjectProperties(std::vector<sdfData*> properties);
    void addRequiredObjectProperty(std::string propertyName);
    // getters for member variables
    bool getReadable();
    bool getWritable();
    bool getNullable();
    bool getObservable();
    bool getReadableDefined() const;
    bool getWritableDefined() const;
    bool getNullableDefined() const;
    bool getObservableDefined() const;
    bool getConstantBool();
    int64_t getConstantInt();
    float getConstantNumber();
    std::string getConstantString();
    sdfData* getConstantObject() const;
    std::string getConstantAsString();
    //const char * getConstantAsCharArray();
    std::string getContentFormat();
    bool getDefaultBool();
    int64_t getDefaultInt();
    float getDefaultNumber();
    std::string getDefaultString();
    sdfData* getDefaultObject() const;
    std::string getDefaultAsString();
    std::vector<std::string> getDefaultArrayAsStringVector();
    //const char * getDefaultAsCharArray();
    bool getDefaultDefined() const;
    bool getConstantDefined() const;
    bool getDefaultIntDefined() const;
    bool getConstantIntDefined() const;
    bool getDefaultBoolDefined() const;
    bool getConstantBoolDefined() const;
    //std::vector<bool> getEnumBool();
    //std::vector<int> getEnumInt();
    //std::vector<float> getEnumNumber();
    std::vector<std::string> getEnumString();
    bool isExclusiveMaximumBool();
    float getExclusiveMaximumNumber();
    bool isExclusiveMinimumBool();
    float getExclusiveMinimumNumber();
    jsonSchemaFormat getFormat();
    float getMaximum();
    float getMaxItems();
    float getMaxItemsOfRef();
    float getMaxLength();
    float getMinimum();
    float getMinItems();
    float getMinItemsOfRef();
    float getMinLength();
    int64_t getMinInt() const;
    uint64_t getMaxInt() const;
    bool getMinIntSet() const;
    bool getMaxIntSet() const;
    float getMultipleOf();
    std::string getPattern();
    float getScaleMaximum();
    float getScaleMinimum();
    sdfSubtype getSubtype();
    jsonDataType getSimpType();
    std::string getType();
    bool getUniqueItems();
    bool getUniqueItemsDefined() const;
    std::string getUnits();
    const char* getUnitsAsArray() const;
    std::vector<bool> getConstantBoolArray() const;
    std::vector<bool> getDefaultBoolArray() const;
    std::vector<int64_t> getConstantIntArray() const;
    std::vector<int64_t> getDefaultIntArray() const;
    std::vector<float> getConstantNumberArray() const;
    std::vector<float> getDefaultNumberArray() const;
    std::vector<std::string> getConstantStringArray() const;
    std::vector<std::string> getDefaultStringArray() const;
    sdfCommon* getParentCommon() const;
    sdfCommon* getParent() const;
    sdfData* getItemConstr() const;
    sdfData* getItemConstrOfRefs() const;
    bool isItemConstr() const;
    bool isObjectProp() const;
    std::vector<sdfData*> getChoice() const;
    std::vector<sdfData*> getObjectProperties() const;
    std::vector<sdfData*> getObjectPropertiesOfRefs() const;
    std::vector<std::string> getRequiredObjectProperties() const;
    sdfData* getThisAsSdfData();
    //(sdfCommon *child) const;
    // parsing
    void parseDefault(const char *value);
    void parseDefaultArray(lys_node_leaflist *node);
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);
    nlohmann::json dataToJson(nlohmann::json prefix);
    sdfData* jsonToData(nlohmann::json input);

private:
    // TODO: use a C union for constant and default values, maximum, minimum?
    bool constDefined;
    bool defaultDefined;
    jsonDataType simpleType;
    //std::vector<jsonDataType> unionTypes;
    std::string derType;
    // only fill for type number
    //std::vector<float> enumNumber;
    float constantNumber;
    float defaultNumber;
    // only fill for type string
    std::vector<std::string> enumString;
    std::string constantString;
    std::string defaultString;
    // only fill for type boolean
    bool constantBool;
    bool defaultBool;
    bool constBoolDefined;
    bool defaultBoolDefined;
    // only fill for type integer
    //std::vector<int> enumInt;
    int64_t constantInt;
    int64_t defaultInt;
    bool constIntDefined;
    bool defaultIntDefined;
    // only fill for type array
    std::vector<bool> constantBoolArray;
    std::vector<bool> defaultBoolArray;
    std::vector<int64_t> constantIntArray;
    std::vector<int64_t> defaultIntArray;
    std::vector<float> constantNumberArray;
    std::vector<float> defaultNumberArray;
    std::vector<std::string> constantStringArray;
    std::vector<std::string> defaultStringArray;
    sdfData *constantObject;
    sdfData *defaultObject;
    /*std::vector<std::vector<auto>> enumArray;
    std::vector<auto> constantArray;
    std::vector<auto> defaultArray;*/
    //const char *constantAsCharArray;
    //const char *defaultAsCharArray;
    double minimum;
    double maximum;
    int64_t minInt;
    uint64_t maxInt;
    bool minIntSet;
    bool maxIntSet;
    float exclusiveMinimum_number; // TODO: number > exclusiveMin vs >=
    float exclusiveMaximum_number;
    bool exclusiveMinimum_bool;
    bool exclusiveMaximum_bool;
    float multipleOf;
    float minLength;
    float maxLength;
    std::string pattern;
    jsonSchemaFormat format;
    float minItems;
    float maxItems;
    bool uniqueItems;
    bool uniqueItemsDefined;
    sdfData *item_constr;
    // SDF-defined data qualities
    std::string units;
    float scaleMinimum;
    float scaleMaximum;
    bool readable;
    bool writable;
    bool observable;
    bool nullable;
    bool readableDefined;
    bool writableDefined;
    bool observableDefined;
    bool nullableDefined;
    std::string contentFormat;
    // TODO: rename sdfSubtype to sdfType?
    sdfSubtype subtype;
    sdfCommon *parent;
    std::vector<sdfData*> objectProperties;
    std::vector<std::string> requiredObjectProperties;
    std::vector<sdfData*> sdfChoice;
};

class sdfEvent : virtual public sdfObjectElement
{
public:
    sdfEvent(std::string _name = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            sdfData* _outputData = NULL,
            std::vector<sdfData*> _datatypes = {});
    virtual ~sdfEvent();
    // setters
    void setOutputData(sdfData *outputData);
    void addDatatype(sdfData *datatype);
    // getters
    std::vector<sdfData*> getDatatypes();
    sdfData* getOutputData();
    //bool hasChild(sdfCommon *child) const;
    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);
    nlohmann::json eventToJson(nlohmann::json prefix);
    sdfEvent* jsonToEvent(nlohmann::json input);
private:
    sdfData *outputData;
    std::vector<sdfData*> datatypes;
};

class sdfAction : virtual public sdfObjectElement
{
public:
    sdfAction(std::string _name = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            sdfData* _inputData = NULL,
            std::vector<sdfData*> _requiredInputData = {},
            sdfData* _outputData = NULL,
            std::vector<sdfData*> _datatypes = {});
    virtual ~sdfAction();
    // setters
    void setInputData(sdfData* inputData);
    void addRequiredInputData(sdfData* requiredInputData);
    void setOutputData(sdfData* outputData);
    void addDatatype(sdfData *datatype);
    // getters
    sdfData* getInputData();
    std::vector<sdfData*> getRequiredInputData();
    sdfData* getOutputData();
    std::vector<sdfData*> getDatatypes();
    //(sdfCommon *child) const;
    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);
    nlohmann::json actionToJson(nlohmann::json prefix);
    sdfAction* jsonToAction(nlohmann::json input);

private:
    sdfData *inputData;
    std::vector<sdfData*> requiredInputData; // TODO: obsolete?
    sdfData *outputData;
    std::vector<sdfData*> datatypes;
};

class sdfProperty : public sdfData, virtual public sdfObjectElement
{
public:
    sdfProperty(std::string _name = "",
            std::string _description = "",
            jsonDataType _type = json_type_undef,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
             sdfObject *_parentObject = NULL);
    sdfProperty(sdfData& data);
    // getters
    sdfCommon* getParent() const;
    //(sdfCommon *child) const;
    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);
    nlohmann::json propertyToJson(nlohmann::json prefix);
    sdfProperty* jsonToProperty(nlohmann::json input);
};

class sdfObject : virtual public sdfCommon
{
public:
    sdfObject(std::string _name = "", std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            std::vector<sdfProperty*> _properties = {},
            std::vector<sdfAction*> _actions = {},
            std::vector<sdfEvent*> _events = {},
            std::vector<sdfData*> _datatypes = {},
            sdfThing *_parentThing = NULL);
    virtual ~sdfObject();
    // setters
    void setInfo(sdfInfoBlock *_info);
    void setNamespace(sdfNamespaceSection *_ns);
    void addProperty(sdfProperty *property);
    void addAction(sdfAction *action);
    void addEvent(sdfEvent *event);
    void addDatatype(sdfData *datatype);
    void setParentThing(sdfThing *parentThing);
    // getters
    sdfInfoBlock* getInfo();
    sdfNamespaceSection* getNamespace();
    std::vector<sdfProperty*> getProperties();
    std::vector<sdfAction*> getActions();
    std::vector<sdfEvent*> getEvents();
    std::vector<sdfData*> getDatatypes();
    sdfThing* getParentThing() const;
    sdfCommon* getParent() const;
    //(sdfCommon *child) const;
    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);
    nlohmann::json objectToJson(nlohmann::json prefix,
            bool print_info_namespace = true);
    std::string objectToString(bool print_info_namespace = true);
    void objectToFile(std::string path);
    sdfObject* jsonToObject(nlohmann::json input, bool testForThing = false);
    sdfObject* fileToObject(std::string path, bool testForThing = false);

private:
    nlohmann::json actionToJson();
    nlohmann::json eventToJson();
    sdfInfoBlock *info;
    sdfNamespaceSection *ns;
    std::vector<sdfProperty*> properties;
    std::vector<sdfAction*> actions;
    std::vector<sdfEvent*> events;
    std::vector<sdfData*> datatypes;
    sdfThing *parent;
};

class sdfThing : virtual public sdfCommon
{
public:
    sdfThing(std::string _name = "", std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            std::vector<sdfThing*> _things = {},
            std::vector<sdfObject*> _objects = {},
            sdfThing *_parentThing = NULL);
    virtual ~sdfThing();
    // setters
    void setInfo(sdfInfoBlock *_info);
    void setNamespace(sdfNamespaceSection *_ns);
    void addThing(sdfThing *thing);
    void addObject(sdfObject *object);
    void setParentThing(sdfThing *parentThing);
    // getters
    sdfInfoBlock* getInfo() const;
    sdfNamespaceSection* getNamespace() const;
    std::vector<sdfThing*> getThings() const;
    std::vector<sdfObject*> getObjects() const;
    sdfThing* getParentThing() const;
    sdfCommon* getParent() const;
    //(sdfCommon *child) const;
    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);
    nlohmann::json thingToJson(nlohmann::json prefix,
            bool print_info_namespace = false);
    std::string thingToString(bool print_info_namespace = true);
    void thingToFile(std::string path);
    sdfThing* jsonToThing(nlohmann::json input, bool nested = false);
    sdfThing* jsonToNestedThing(nlohmann::json input);
    sdfThing* fileToThing(std::string path);

private:
    sdfInfoBlock *info;
    sdfNamespaceSection *ns;
    std::vector<sdfThing*> childThings;
    std::vector<sdfObject*> childObjects;
    sdfThing *parent;

};

/*
 * Products may be composed of Objects and Things at the high level,
 * and may include their own definitions of Properties, Actions, and
 * Events that can be used to extend or complete the included Object
 * definitions. Product definitions may set optional defaults and
 * constant values for specific use cases, for example units, range,
 * and scale settings for properties, or available parameters for Actions
 */
class sdfProduct : sdfThing
{
    // ???
};

class sdfFile
{
public:
    sdfFile();
    ~sdfFile();
    // setters
    void setInfo(sdfInfoBlock *_info);
    void setNamespace(sdfNamespaceSection *_ns);
    void addThing(sdfThing *thing);
    void addObject(sdfObject *object);;
    void addProperty(sdfProperty *property);
    void addAction(sdfAction *action);
    void addEvent(sdfEvent *event);
    void addDatatype(sdfData *datatype);
    // getters
    sdfInfoBlock* getInfo() const;
    sdfNamespaceSection* getNamespace() const;
    std::vector<sdfThing*> getThings() const;
    std::vector<sdfObject*> getObjects() const;
    std::vector<sdfProperty*> getProperties();
    std::vector<sdfAction*> getActions();
    std::vector<sdfEvent*> getEvents();
    std::vector<sdfData*> getDatatypes();
    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);
    nlohmann::json toJson(nlohmann::json prefix);
    std::string toString();
    void toFile(std::string path);
    sdfFile* fromJson(nlohmann::json input);
    sdfFile* fromFile(std::string path);
private:
    sdfInfoBlock *info;
    sdfNamespaceSection *ns;
    std::vector<sdfThing*> things;
    std::vector<sdfObject*> objects;
    std::vector<sdfProperty*> properties;
    std::vector<sdfAction*> actions;
    std::vector<sdfEvent*> events;
    std::vector<sdfData*> datatypes;
};

#endif

