#ifndef SDF_H
#define SDF_H

#include <iostream>
#include <fstream>
#include<sstream>
#include<stdio.h>
#include<string>
#include<vector>
#include <cmath>
#include <regex>
#include <nlohmann/json.hpp>

// for convenience
//using json = nlohmann::json;
//using namespace std;

enum jsonDataType
{
    json_number,
    json_string,
    json_boolean,
    json_integer,
    json_array,
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

class sdfCommon;
class sdfThing;
class sdfObject;

std::string jsonDTypeToString(jsonDataType type);
jsonDataType stringToJsonDType(std::string str);
sdfCommon* refToCommon(std::string ref);
std::string  correctValue(std::string val);


// sdfObject, sdfProperty, sdfAction, sdfEvent and sdfData are sdfCommons
class sdfCommon
{
public:
    sdfCommon(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {});
    // getters
    std::string getDescription();
    std::string getLabel();
    sdfCommon* getReference();
    std::vector<sdfCommon*> getRequired();
    //sdfCommon* getParentCommon();
    // setters
    void setLabel(std::string _label);
    void setDescription(std::string dsc);
    void addRequired(sdfCommon *common);
    void setReference(sdfCommon *common);
    //void setParentCommon(sdfCommon *parentCommon);
    // printing
    virtual std::string generateReferenceString() = 0;
    nlohmann::json commonToJson(nlohmann::json prefix);
    void jsonToCommon(nlohmann::json input);
private:
    std::string description;
    std::string label;
    sdfCommon *reference;
    std::vector<sdfCommon*> required;
    //sdfCommon *parent;
};

class sdfObjectElement : virtual public sdfCommon
{
public:
    sdfObjectElement(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL);
    sdfObject* getParentObject();
    void setParentObject(sdfObject *parentObject);
    std::string generateReferenceString();
private:
    sdfObject *parent;
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
    sdfNamespaceSection(std::map<std::string, std::string> _namespaces = {},
                        std::string _default_ns = "");
    // getters
    std::map<std::string, std::string> getNamespaces();
    std::string getDefaultNamespace();
    // parsing
    nlohmann::json namespaceToJson(nlohmann::json prefix);
    sdfNamespaceSection* jsonToNamespace(nlohmann::json input);
private:
    std::map<std::string, std::string> namespaces;
    std::string default_ns;
};

class sdfData : virtual public sdfCommon
{
public:
    sdfData();
    sdfData(std::string _label,
            std::string _description,
            std::string _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL);
    sdfData(std::string _label,
            std::string _description,
            jsonDataType _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL);
    // Setters for member variables (different for types)
    // check which of the sdfData data qualities are designated for which
    // data type (number, string, ...)
    void setNumberData(float _constant = NAN,
            float _default = NAN,
            float _min = NAN,
            float _max = NAN,
            float _multipleOf = NAN,
            std::vector<float> _enum = {});
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
                     bool defineDefault = false,
                     std::vector<bool> _enum = {});
    void setIntData(int _constant = -1,
                    bool defineConst = false,
                    int _default = -1,
                    bool defineDefault = false,
                    float _min = NAN,
                    float _max = NAN,
                    std::vector<int> _enum = {});
    void setArrayData(float _minItems,
                      float _maxItems,
                        bool _uniqueItems,
                      std::string item_type,
                      sdfCommon *ref = NULL);
    void setArrayData(std::vector<std::string> enm,
                      float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                         std::string item_type = "string",
                      sdfCommon *ref = NULL,
                      float minLength = NAN,
                      float maxLength = NAN,
                      jsonSchemaFormat format = json_format_undef);
    void setArrayData(std::vector<float> enm,
                      float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                      std::string item_type = "number",
                      sdfCommon *ref = NULL,
                      float min = NAN,
                      float max = NAN);
    void setArrayData(std::vector<int> enm,
                      float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                      std::string item_type = "integer",
                      sdfCommon *ref = NULL,
                      float min = NAN,
                      float max = NAN);
    // other setters
    void setSimpType(jsonDataType _type);
    void setType(std::string _type);
    void setType(jsonDataType _type);
    void setDerType(std::string _type);
    void setUnits(std::string _units, float _scaleMin = NAN, float _scaleMax = NAN);
    void setReadWrite(bool _readable, bool _writable);
    void setObserveNull(bool _observable, bool _nullable);
    void setFormat(jsonSchemaFormat _format);
    void setSubtype(sdfSubtype _subtype);
    void setMaxItems(float maxItems);
    void setMinItems(float minItems);
    void setPattern(std::string pattern);
    void setConstantBool(bool constantBool);
    void setConstantInt(int constantInt);
    void setConstantNumber(float constantNumber);
    void setConstantString(std::string constantString);
    void setDefaultBool(bool defaultBool);
    void setDefaultInt(int defaultInt);
    void setDefaultNumber(float defaultNumber);
    void setDefaultString(std::string defaultString);
    void setConstantArray(std::vector<std::string> constantArray);
    void setDefaultArray(std::vector<std::string> defaultArray);
    void setParentCommon(sdfCommon *parentCommon);
    // getters for member variables
    bool getReadable();
    bool getWritable();
    bool getNullable();
    bool getObservable();
    bool getConstantBool();
    int getConstantInt();
    float getConstantNumber();
    std::string getConstantString();
    std::string getContentFormat();
    bool getDefaultBool();
    int getDefaultInt();
    float getDefaultNumber();
    std::string getDefaultString();
    std::vector<bool> getEnumBool();
    std::vector<int> getEnumInt();
    std::vector<float> getEnumNumber();
    std::vector<std::string> getEnumString();
    bool isExclusiveMaximumBool();
    float getExclusiveMaximumNumber();
    bool isExclusiveMinimumBool();
    float getExclusiveMinimumNumber();
    jsonSchemaFormat getFormat();
    float getMaximum();
    float getMaxItems();
    float getMaxLength();
    float getMinimum();
    float getMinItems();
    float getMinLength();
    float getMultipleOf();
    std::string getPattern();
    float getScaleMaximum();
    float getScaleMinimum();
    sdfSubtype getSubtype();
    jsonDataType getSimpType();
    std::string getType();
    bool getUniqueItems();
    std::string getUnits();
    std::vector<std::string> getConstantArray();
    std::vector<std::string> getDefaultArray();
    sdfCommon* getParentCommon();
    // parsing
    std::string generateReferenceString();
    nlohmann::json dataToJson(nlohmann::json prefix);
    sdfData* jsonToData(nlohmann::json input);

private:
    bool constDefined;
    bool defaultDefined;
    jsonDataType simpleType;
    //std::vector<jsonDataType> unionTypes;
    std::string derType;
    // only fill for type number
    std::vector<float> enumNumber;
    float constantNumber;
    float defaultNumber;
    // only fill for type string
    std::vector<std::string> enumString;
    std::string constantString;
    std::string defaultString;
    // only fill for type boolean
    std::vector<bool> enumBool; // does this even make sense?
    bool constantBool;
    bool defaultBool;
    // only fill for type integer
    std::vector<int> enumInt;
    int constantInt;
    int defaultInt;
    // only fill for type array
    // TODO: find a way to represent not only string arrays as const/default
    std::vector<std::string> constantArray;
    std::vector<std::string> defaultArray;
    /*std::vector<std::vector<auto>> enumArray;
    std::vector<auto> constantArray;
    std::vector<auto> defaultArray;*/
    float minimum;
    float maximum;
    float exclusiveMinimum_number; // TODO: I don't get this
    float exclusiveMaximum_number;
    bool exclusiveMinimum_bool;
    bool exclusiveMaximum_bool;
    float multipleOf;
    float minLength;
    float maxLength;
    std::string pattern; // regex?
    jsonSchemaFormat format;
    float minItems;
    float maxItems;
    bool uniqueItems;
    sdfData *item_constr;
    // SDF-defined data qualities
    std::string units;
    float scaleMinimum;
    float scaleMaximum;
    bool readable;
    bool writable;
    bool observable;
    bool nullable;
    std::string contentFormat;
    sdfSubtype subtype;
    sdfCommon *parent;
};

class sdfEvent : virtual public sdfObjectElement
{
public:
    sdfEvent(std::string _label = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            std::vector<sdfData*> _outputData = {},
            std::vector<sdfData*> _datatypes = {});
    // setters
    void addOutputData(sdfData *outputData);
    void addDatatype(sdfData *datatype);
    // getters
    std::vector<sdfData*> getDatatypes();
    std::vector<sdfData*> getOutputData();
    // parsing
    std::string generateReferenceString();
    nlohmann::json eventToJson(nlohmann::json prefix);
    sdfEvent* jsonToEvent(nlohmann::json input);
private:
    std::vector<sdfData*> outputData;
    std::vector<sdfData*> datatypes;
};

class sdfAction : virtual public sdfObjectElement
{
public:
    sdfAction(std::string _label = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            std::vector<sdfData*> _inputData = {},
            std::vector<sdfData*> _requiredInputData = {},
            std::vector<sdfData*> _outputData = {},
            std::vector<sdfData*> _datatypes = {});
    // setters
    void addInputData(sdfData* inputData);
    void addRequiredInputData(sdfData* requiredInputData);
    void addOutputData(sdfData* outputData);
    void addDatatype(sdfData *datatype);
    // getters
    std::vector<sdfData*> getInputData();
    std::vector<sdfData*> getRequiredInputData();
    std::vector<sdfData*> getOutputData();
    std::vector<sdfData*> getDatatypes();
    // parsing
    std::string generateReferenceString();
    nlohmann::json actionToJson(nlohmann::json prefix);
    sdfAction* jsonToAction(nlohmann::json input);

private:
    std::vector<sdfData*> inputData;
    std::vector<sdfData*> requiredInputData;
    std::vector<sdfData*> outputData;
    std::vector<sdfData*> datatypes;
};

class sdfProperty : public sdfData, virtual public sdfObjectElement
{
public:
    sdfProperty(std::string _label = "",
            std::string _description = "",
            jsonDataType _type = {},
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
             sdfObject *_parentObject = NULL);
    // parsing
    std::string generateReferenceString();
    nlohmann::json propertyToJson(nlohmann::json prefix);
    sdfProperty* jsonToProperty(nlohmann::json input);
};

class sdfObject : virtual public sdfCommon
{
public:
    sdfObject(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
            std::vector<sdfProperty*> _properties = {},
            std::vector<sdfAction*> _actions = {}, std::vector<sdfEvent*> _events = {},
            std::vector<sdfData*> _datatypes = {}, sdfThing *_parentThing = NULL);
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
    sdfThing* getParentThing();
    // parsing
    std::string generateReferenceString();
    nlohmann::json objectToJson(nlohmann::json prefix, bool print_info_namespace = true);
    std::string objectToString(bool print_info_namespace = true);
    void objectToFile(std::string path);
    sdfObject* jsonToObject(nlohmann::json input);
    sdfObject* fileToObject(std::string path);

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
    sdfThing(std::string _label = "", std::string _description = "",
            sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
            std::vector<sdfThing*> _things = {}, std::vector<sdfObject*> _objects = {},
            sdfThing *_parentThing = NULL);
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
    // parsing
    std::string generateReferenceString();
    nlohmann::json thingToJson(nlohmann::json prefix, bool print_info_namespace = false);
    std::string thingToString(bool print_info_namespace = true);
    void thingToFile(std::string path);
    sdfThing* jsonToThing(nlohmann::json input); // TODO: return void?
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

#endif

