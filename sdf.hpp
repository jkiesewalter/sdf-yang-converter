/*!
 * @file sdf.hpp
 * @brief The SDF serialiser/deserialiser programming interface header
 *
 * This header specifies the programming interface of the SDF
 * serialiser/deserialiser.
 */

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

/**
 * Enumeration representing JSON built-in data types
 */
enum jsonDataType
{
    json_number,    /*!< Type number */
    json_string,    /*!< Type string */
    json_boolean,   /*!< Type boolean */
    json_integer,   /*!< Type integer */
    json_array,     /*!< Type array */
    json_object,    /*!< Type object */
    json_type_undef /*!< Type is undefined */
};

/**
 * Enumeration representing JSON schema formats
 */
enum jsonSchemaFormat
{
    json_date_time,     /*!< Format representing a date and time point */
    json_date,          /*!< Format representing a date */
    json_time,          /*!< Format representing a time point */
    json_uri,           /*!< Format representing a URI */
    json_uri_reference, /*!< Format representing a URI reference */
    json_uuid,          /*!< Format representing a UUID */
    json_format_undef   /*!< Format is undefined */
};

/**
 * Enumeration of sdfTypes
 */
enum sdfSubtype
{
    sdf_byte_string,  /*!< sdfType representing a sequence of zero or more bytes */
    sdf_unix_time,    /*!< sdfType representing point in civil time */
    sdf_subtype_undef /*!< sdfType is undefined */
};

/**
 * Enumeration used to specify whether a function deals with references for the
 * sdfRef or sdfRequired quality
 *
 * @sa assignRefs()
 */
enum refOrReq
{
    REF, /*!< sdfReference */
    REQ  /*!< sdfRequired */
};

class sdfCommon;
class sdfThing;
class sdfObject;
class sdfProperty;
class sdfData;
class sdfFile;

/**
 * Returns a string corresponding to the given JSON data type
 *
 * @param type The type to convert to a string
 *
 * @return The type as string
 *
 * @sa jsonDataType
 */
std::string jsonDTypeToString(jsonDataType type);

/**
 * Deduces the JSON data type from a given string
 *
 * @param str The input string
 *
 * @return The corresponding JSON data type
 *
 * @sa jsonDataType
 */
jsonDataType stringToJsonDType(std::string str);

/**
 * Takes a reference string and optionally a prefix and returns a pointer to the
 * referenced sdfCommon object, if it exists in the global storage of existing
 * definitions.
 *
 * @param ref      The reference string (sdfRef)
 * @param nsPrefix Optionally a prefix as given in the namespace section
 *
 * @return If the referenced sdfCommon is found in global storage, a pointer to
 *         it is returned, otherwise NULL is returned.
 */
sdfCommon* refToCommon(std::string ref, std::string nsPrefix = "");

/**
 * Assigns sdfRef references as pointers to the object to sdfCommon objects
 *
 * @param unassignedRefs A vector with tuples of reference strings and pointers
 *                       to sdfCommon objects that use them in their sdfRef
 *                       quality
 *
 * @return The references that could not be assigned because they were not found
 */
std::vector<std::tuple<std::string, sdfCommon*>> assignRefs(
        std::vector<std::tuple<std::string, sdfCommon*>> unassignedRefs);

/**
 * Replaced spaces in a string with underscores ("_").
 *
 * @param val The original string
 *
 * @return The corrected string
 */
std::string  correctValue(std::string val);

/**
 * Validates a given JSON object (SDF model) against a JSON schema in a
 * specified file.
 *
 * @param sdf            The JSON object to be validated (usually contains an
 *                       SDF model)
 * @param schemaFileName The file name of file containing the JSON schema
 *                       (set to the SDF validation JSON schema by default)
 *
 * @return The validation result
 */
bool validateJson(nlohmann::json sdf,
        std::string schemaFileName = "sdf-validation.cddl");

/**
 * Validates an SDF model in a specified file against a JSON schema in a
 * specified file.
 *
 * @param fileName       The file name of the file containing the JSON object to
 *                       be validated (usually contains an SDF model)
 * @param schemaFileName The file name of file containing the JSON schema
 *                       (set to the SDF validation JSON schema by default)
 *
 * @return The validation result
 */
bool validateFile(std::string fileName,
        std::string schemaFileName = "sdf-validation.cddl");

#define INDENT_WIDTH 2 /**< The indent width of output SDF JSON files */

/**
 * The sdfCommon class is used as a base to the sdfObject, sdfProperty,
 * sdfAction, sdfEvent and sdfData classes.
 */
class sdfCommon
{
public:
    /**
     * The sdfCommon constructor
     *
     * @param _name        The name
     * @param _description The description
     * @param _reference   sdfRef quality: a pointer to another sdfCommon referenced through
     *                     the sdfRef quality
     * @param _required    sdfRequired: a vector of pointers to subordinate
     *                     sdfCommons that are marked as required through the
     *                     sdfRequired quality
     * @param _file        A pointer to the file (model) that the sdfCommon
     *                     belongs to
     */
    sdfCommon(std::string _name = "",
              std::string _description = "",
              sdfCommon *_reference = NULL,
              std::vector<sdfCommon*> _required = {},
              sdfFile *_file = NULL);
    /**
     * The (virtual) sdfCommon destructor
     */
    virtual ~sdfCommon();
    // getters
    /**
     * Getter function for the name
     * @return The name
     */
    std::string getName() const;
    /**
     * Getter function for the name as a C-array
     * @return The name as a C-array
     */
    const char* getNameAsArray() const;
    /**
     * Getter function for the description
     * @return The description
     */
    std::string getDescription();
    /**
     * Getter function for the description as a C-array
     * @return The description as a C-array
     */
    const char* getDescriptionAsArray();

    /**
     * Getter function for the label
     * @return The label
     */
    std::string getLabel();

    /**
     * Getter function for the label as a C-array
     * @return The label as a C-array
     */
    const char* getLabelAsArray();

    /**
     * Getter function for the referenced definition
     * (content of the sdfRef quality)
     *
     * @return The content of the sdfRef quality
     *
     * @sa reference
     */
    sdfCommon* getReference();

    /**
     * Getter function for the required subordinate definitions
     * (content of the sdfRequired quality)
     *
     * @return The content of the sdfRequired quality
     *
     * @sa required
     */

    std::vector<sdfCommon*> getRequired();
    /**
     * Getter function for the referenced definition cast to a sdfData pointer
     * (content of the sdfRef quality)
     * @return The content of the sdfRef quality cast to a sdfData pointer
     *         (NULL if not possible)
     */
    sdfData* getSdfDataReference() const;

    /**
     * Getter function for the referenced definition cast to a sdfProperty
     * pointer (content of the sdfRef quality)
     * @return The content of the sdfRef quality cast to a sdfProperty pointer
     *         (NULL if not possible)
     */
    sdfData* getSdfPropertyReference() const;

    /**
     * Getter function for this sdfCommon cast to a sdfData pointer
     * @return This sdfCommon cast to a sdfData pointer
     *         (NULL if not possible)
     */
    sdfData* getThisAsSdfData();

    /**
     * Getter function for the file/model the sdfCommon belongs to
     * @return A pointer to the sdfFile this sdfCommon belongs to
     */
    sdfFile* getParentFile() const;

    /**
     * Getter function for the file/model the sdfCommon belongs to
     * @return A pointer to the sdfFile this sdfCommon belongs to
     *
     * @sa parentFile
     */
    sdfFile* getTopLevelFile();

    /**
     * Getter function for the parent sdfCommon.
     * @return The parent object in the respecitive class cast to an sdfCommon
     * pointer.
     */
    virtual sdfCommon* getParent() const = 0;

    /**
     * Getter function for the default namespace that is attached to the
     * sdfFile the sdfCommon belongs to
     * @return The default namespace
     */
    std::string getDefaultNamespace();

    /**
     * Getter function for all namespaces of the sdfFile the sdfCommon belongs
     * to
     * @return The namespaces map
     */
    std::map<std::string, std::string> getNamespaces();

    // setters
    /**
     * Setter function for the name member variable
     * @param _name The new value for the name member variable
     *
     * @sa name
     */
    void setName(std::string _name);

    /**
     * Setter function for the label member variable
     * @param _label The new value for the label member variable
     *
     * @sa label
     */

    void setLabel(std::string _label);
    /**
     * Setter function for the dsc member variable
     * @param dsc The value to set the dsc member variable to
     *
     * @sa dsc
     */
    void setDescription(std::string dsc);

    /**
     * Setter function to add an element to the required member vector
     * @param common The value to add to the the required member vector
     *
     * @sa required
     */
    void addRequired(sdfCommon *common);

    /**
     * Setter function for the reference member variable
     * @param common The value to set the reference member variable to
     *
     * @sa reference
     */

    void setReference(sdfCommon *common);
    /**
     * Setter function for the parentFile member variable
     * @param file The value to set the parentFile member variable to
     *
     * @sa parentFile
     */
    void setParentFile(sdfFile *file);

    // printing
    /**
     * Generate a string that contains a reference to this sdfCommon as
     * required by the sdfRef quality.
     *
     * This function works by calling this sdfCommon's parent's version of the
     * function with this sdfCommon as a parameter.
     *
     * @param import Set to true if this sdfCommon is referenced from another
     *               model and the reference string needs a prefix
     *
     * @return The reference string to refer to this sdfCommon
     */
    std::string generateReferenceString(bool import = false);

    /**
     * Generate a string that contains a reference to a
     * given subordinate sdfCommon as required by the sdfRef quality.
     *
     * This function cannot be called on child directly because the class of
     * the superordinate element of child must be known since it is stated in
     * the reference string (e.g. "#/sdfObject/sensor/sdfData/temperatureData").
     * Each class has its own version of this function to be able to append the
     * appropriate class name.
     *
     * @param child  A pointer to the subordinate sdfCommon that the reference
     *               string is to be created for
     * @param import Set to true if this sdfCommon is referenced from another
     *               model and the reference string needs a prefix
     *
     * @return The reference string to refer to child
     */
    virtual std::string generateReferenceString(sdfCommon *child,
            bool import = false) = 0;

    /**
     * Transfer the information from this sdfCommon object into a JSON object
     *
     * @param prefix JSON object to add to (prefix does not mean the same prefix
     *               as in the namespace prefix)
     *
     * @return The completed JSON object
     */
    nlohmann::json commonToJson(nlohmann::json prefix);

    /**
     * Transfer the information from a given JSON object into this sdfCommon
     * object.
     *
     * @param input The input JSON object
     */
    void jsonToCommon(nlohmann::json input);

private:
    /**
     * The name of this SDF definition's representation.
     *
     * Not to be confused with the label which is its own SDF  quality.
     * The name is the string the definition is referred to
     * (e.g. "sdfObject": { "exampleName": {...} }).
     *
     * @sa setName(), getName() and getNameAsArray()
     */
    std::string name;

    /**
     * Represents the description quality.
     * @sa setDescription(), getDescription() and getDescriptionAsArray
     */
    std::string description;

    /**
     * Represents the label quality.
     * @sa setLabel(), getLabel() and getLabelAsArray
     */
    std::string label;

    /**
     * Represents the sdfRef quality.
     * @sa setReference() and getReference()
     */
    sdfCommon *reference;

    /**
     * Represents the sdfRequired quality.
     * @sa setRequired(), addRequired() and getRequired()
     */
    std::vector<sdfCommon*> required;

    /**
     * A pointer to the subordinate file the sdfCommon belongs to.
     * @sa setParentFile() and getParent()
     */
    sdfFile *parentFile;
};

/**
 * The sdfObjectElement class inherits from the sdfCommon class and is used to
 * represent classes that can appear as declarations in an sdfObject. It is used
 * as a base to the sdfProperty, sdfAction, sdfEvent classes.
 */
class sdfObjectElement : virtual public sdfCommon
{
public:
    /**
     * The sdfObjectElement constructor
     * @param _name         The name
     * @param _description  The description
     * @param _reference    sdfRef quality: a pointer to another sdfCommon
     *                      referenced through
     *                      the sdfRef quality
     * @param _required     sdfRequired: a vector of pointers to subordinate
     *                      sdfCommons that are marked as required through the
     *                      sdfRequired quality
     * @param _parentObject A pointer to the superordinate sdfObject
     */
    sdfObjectElement(std::string _name = "", std::string _description = "",
           sdfCommon *_reference = NULL, std::vector<sdfCommon*> _required = {},
           sdfObject *_parentObject = NULL);

    /**
     * The sdfObjectElement destructor.
     *
     * A destructor is needed to free the pointer to the superordinate
     * sdfObject.
     */
    ~sdfObjectElement();

    /**
     * Getter function
     * @return The value of the parentObject member variable
     * @sa parentObject
     */
    sdfObject* getParentObject() const;

    /**
     * Getter function
     * @return The value of the parentObject member variable cast into  a
     *         sdfCommon pointer
     * @sa parentObject
     */
    sdfCommon* getParent() const;

    /**
     * Setter function
     * @param _parentObject The new value of the parentObject member variable
     * @sa parentObject
     */
    void setParentObject(sdfObject *_parentObject);
    virtual std::string generateReferenceString(sdfCommon *child,
            bool import = false) = 0;
private:
    /**
     *  A pointer to the superordinate sdfObject
     *  @sa setParentObject(), getParent() and getParentObject
     */
    sdfObject *parentObject;
};

/**
 * This class represents the information block in SDF.
 */
class sdfInfoBlock
{
public:
    /**
     * The sdfObjectElement constructor
     * @param _title     The title
     * @param _version   The version
     * @param _copyright Copyright information
     * @param _license   License terms
     */
    sdfInfoBlock(std::string _title = "", std::string _version = "",
                std::string _copyright = "", std::string _license= "");
    // getters
    /**
     * Getter function
     * @return The value of the title member variable
     * @sa title
     */
    std::string getTitle();
    /**
     * Getter function
     * @return The value of the version member variable
     * @sa version
     */
    std::string getVersion();
    /**
     * Getter function
     * @return The value of the copyright member variable
     * @sa copyright
     */
    std::string getCopyright();
    /**
     * Getter function
     * @return The value of the license member variable
     * @sa license
     */
    std::string getLicense();
    /**
     * Getter function
     * @return The value of the title member variable as a C-array
     * @sa title
     */
    const char* getTitleAsArray() const;
    /**
     * Getter function
     * @return The value of the version member variable as a C-array
     * @sa version
     */
    const char* getVersionAsArray() const;
    /**
     * Getter function
     * @return The value of the copyright member variable as a C-array
     * @sa copyright
     */
    const char* getCopyrightAsArray() const;
    /**
     * Getter function
     * @return The value of the license member variable as a C-array
     * @sa license
     */
    const char* getLicenseAsArray() const;
    // parsing
    /**
     * Transfer the information from this sdfInfoBlock object into a JSON object
     *
     * @param prefix JSON object to add to (prefix does not mean the same prefix
     *               as in the namespace prefix)
     *
     * @return The completed JSON object
     */
    nlohmann::json infoToJson(nlohmann::json prefix);
    /**
     * Transfer the information from a given JSON object into this sdfInfoBlock
     * object.
     *
     * @param input The input JSON object
     */
    sdfInfoBlock* jsonToInfo(nlohmann::json input);
private:
    /**
     * The title (for search results)
     *
     * @sa getTitle() and getTitleAsArray()
     */
    std::string title;

    /**
     * The version (format TBD)
     *
     * @sa getVersion() and getVersionAsArray()
     */
    std::string version;

    /**
     * Copyright information
     *
     * @sa getCopyright() and getCopyrightAsArray()
     */
    std::string copyright;

    /**
     * License terms
     *
     * @sa getLicense() and getLicenseAsArray()
     */
    std::string license;
};

/**
 * This class is used to represent the namespace section of an SDF model.
 */
class sdfNamespaceSection
{
public:
    /**
     * The sdfNamespaceSection default constructor
     */
    sdfNamespaceSection();
    /**
     * The sdfNamespaceSection constructor
     * @param _namespaces Short names (prefixes) for each namespace mapped
     *                    to the URI of the namespace
     * @param _default_ns One of the prefixes is used as default namespace
     */
    sdfNamespaceSection(std::map<std::string, std::string> _namespaces,
                        std::string _default_ns);
    // getters
    /**
     * Getter function
     * @return The value of the namespaces member variable
     * @sa namespaces
     */
    std::map<std::string, std::string> getNamespaces();
    /**
     * Getter function
     * @return The value of the namespaces member variable as C-strings
     * @sa namespaces
     */
    std::map<const char*, const char*> getNamespacesAsArrays();
    /**
     * Getter function
     * @return The value of the default_ns member variable
     * @sa default_ns
     */
    std::string getDefaultNamespace();
    /**
     * Getter function
     * @return The value of the default_ns member variable as C-strings
     * @sa default_ns
     */
    const char* getDefaultNamespaceAsArray();
    /**
     * Getter function
     * @return The value of the default_ns member variable if it is not the
     *         empty string, otherwise the first prefix element in the
     *         namespaces map
     * @sa default_ns and namespaces
     */
    std::string getNamespaceString() const;
    /**
     * Getter function
     * @return The value of the namedFiles member variable
     * @sa namedFiles
     */
    std::map<std::string, sdfFile*> getNamedFiles() const;
    // setters
    /**
     * Removes specified namespace element from the namespaces and namedFiles
     * members
     * @param pre The prefix of the namespace element to remove
     * @sa namespaces and namedFiles
     */
    void removeNamespace(std::string pre);
    /**
     * Adds a specified namespace element to the namespaces and namedFiles
     * members
     *
     * @param pre The prefix of the namespace element to add
     * @param ns  The namespace element's URI
     *
     * @sa namespaces and namedFiles
     */
    void addNamespace(std::string pre, std::string ns);
    /**
     * Links foreign namespaces to their sdfFile object by iterating through
     * the namespaces map
     * and searching for the mentioned foreign model's sdfFile representations
     * in the global storage
     */
    void updateNamedFiles();
    // parsing
    /**
     * Transfer the information from this sdfNamespaceSection object into a
     * JSON object
     *
     * @param prefix JSON object to add to (prefix does not mean the same prefix
     *               as in the namespace prefix)
     *
     * @return The completed JSON object
     */
    nlohmann::json namespaceToJson(nlohmann::json prefix);

    /**
     * Transfer the information from a given JSON object into this
     * sdfNamespaceSection
     * object.
     *
     * @param input The input JSON object
     */
    sdfNamespaceSection* jsonToNamespace(nlohmann::json input);

    /**
     * Inserts all definitions of this sdfNamespaceSection
     * into the global definitions storage
     * and adds the default prefix to the path
     */
    void makeDefinitionsGlobal();
private:
    /**
     * Short names (prefixes) for each namespace mapped
     * to the URI of the namespace
     *
     * @sa getNamespaces(), getNamespacesAsArrays(), addNamespace() and
     *     removeNamespace()
     */
    std::map<std::string, std::string> namespaces;

    /**
     * One of the prefixes is used as default namespace (to enable resolving
     * references and thus make the SDF model globally available)
     *
     * @sa getDefaultNamespace(), getDefaultNamespaceAsArray() and
     *     getNamespaceString()
     */
    std::string default_ns;

    /**
     * Maps prefixes to pointers to their respective sdfFile
     *
     * @sa getNamedFiles(), addNamespace() and updateNamedFiles()
     */
    std::map<std::string, sdfFile*> namedFiles;
};

/**
 * This class represents sdfData definitions. It thus has members for all of
 * the data qualities and inherits the common qualities from the sdfCommon
 * class.
 */
class sdfData : virtual public sdfCommon
{
public:
    /**
     * The sdfData default constructor
     */
    sdfData();
    /**
     * The sdfData constructor
     *
     * @param _name         The name
     * @param _description  The description
     * @param _type         The type in the form of a string
     * @param _reference    sdfRef quality: a pointer to another sdfCommon referenced through
     *                      the sdfRef quality
     * @param _required     sdfRequired: a vector of pointers to subordinate
     *                      sdfCommons that are marked as required through the
     *                      sdfRequired quality
     * @param _parentCommon A pointer to the superordinate sdfCommon object
     * @param _choice       sdfChoice: A vector of sdfData pointers that
     *                      represent the elements of the sdfChoice
     */
    sdfData(std::string _name,
            std::string _description,
            std::string _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL,
            std::vector<sdfData*> _choice = {});
    /**
     * The sdfData constructor
     *
     * @param _name         The name
     * @param _description  The description
     * @param _type         The type as JSON data type enumeration
     * @param _reference    sdfRef quality: a pointer to another sdfCommon referenced through
     *                      the sdfRef quality
     * @param _required     sdfRequired: a vector of pointers to subordinate
     *                      sdfCommons that are marked as required through the
     *                      sdfRequired quality
     * @param _parentCommon A pointer to the superordinate sdfCommon object
     * @param _choice       sdfChoice: A vector of sdfData pointers that
     *                      represent the elements of the sdfChoice
     */
    sdfData(std::string _name,
            std::string _description,
            jsonDataType _type,
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfCommon *_parentCommon = NULL,
            std::vector<sdfData*> _choice = {});

    /**
     * The sdfData copy constructor
     * @param data The address of the sdfData object to copy
     */
    sdfData(sdfData &data);

    /**
     * The sdfData copy constructor for casting sdfProperty objects
     * @param prop The address of the sdfProperty object to copy
     */
    sdfData(sdfProperty &prop);

    /**
     * The sdfData destructor.
     *
     * A destructor is needed to free the pointers associated with various
     * member variables.
     */
    ~sdfData();

    // Setters for member variables (different for types)
    // check which of the sdfData data qualities are designated for which
    // data type (number, string, ...)
    /**
     * Setter function for member variables associated with the number type
     *
     * @param _constant   A constant value
     * @param _default    A default value
     * @param _min        A minimum number
     * @param _max        A maximum number
     * @param _multipleOf A value for the resolution of the number
     *
     * @sa constantNumber, defaultNumber, minimum, maximum and multipleOf
     */
    void setNumberData(float _constant = NAN,
            float _default = NAN,
            float _min = NAN,
            float _max = NAN,
            float _multipleOf = NAN);

    /**
     * Setter function for member variables associated with the string type
     *
     * @param _constant   A constant value
     * @param _default    A default value
     * @param _minLength  A minimum length
     * @param _maxLength  A maximum length
     * @param _pattern    A regular expression pattern
     * @param _format     A JSON schema format
     * @param _enum       A string enumeration
     *
     * @sa constantString, defaultString, minLength, maxLength, pattern and
     *     enumString
     */
    void setStringData(std::string _constant = "",
                       std::string _default = "",
                       float _minLength = NAN,
                       float _maxLength = NAN,
                       std::string _pattern = "",
                       jsonSchemaFormat _format = json_format_undef,
                       std::vector<std::string> _enum = {});

    /**
     * Setter function for member variables associated with the boolean type
     *
     * @param _constant      A constant value
     * @param defineConst    Whether or not constBoolDefined should be set
     * @param _default       A default value
     * @param defineDefault  Whether or not defaultBoolDefined should be set
     *
     * @sa constantBool, defaultBool, constBoolDefined and defaultBoolDefined
     */
    void setBoolData(bool _constant = false,
                     bool defineConst = false,
                     bool _default = false,
                     bool defineDefault = false);

    /**
     * Setter function for member variables associated with the integer type
     *
     * @param _constant      A constant value
     * @param defineConst    Whether or not constIntDefined should be set
     * @param _default       A default value
     * @param defineDefault  Whether or not defaultIntDefined should be set
     * @param _min           A minimum value (is float to be able to set to NaN)
     * @param _max           A maximum value (is float to be able to set to NaN)
     *
     * @sa constantInt, defaultInt, constIntDefined, defaultIntDefined, minInt
     *     and maxInt
     */
    void setIntData(int _constant = -1,
                    bool defineConst = false,
                    int _default = -1,
                    bool defineDefault = false,
                    float _min = NAN,
                    float _max = NAN);

    /**
     * Setter function for member variables associated with the array type
     *
     * @param _minItems    Minimal number of items in the array
     * @param _maxItems    Maximal number of items in the array
     * @param _uniqueItems Whether or not the items should be unique
     * @param _itemConstr  The constraints on the array items (represented by
     *                     another sdfData object)
     *
     * @sa minItems, maxItems, uniqueItems and item_constr
     */
    void setArrayData(float _minItems = NAN,
                      float _maxItems = NAN,
                      bool _uniqueItems = NAN,
                      sdfData *_itemConstr = NULL);

    // other setters
    /**
     * Setter function
     * @param _type The new value of the simpType member variable
     * @sa simpType
     */
    void setSimpType(jsonDataType _type);

    /**
     * Setter function
     * @param _type The new value of the simpType and derType member variables,
     *              given in JSON data type format
     * @sa simpleType and derType
     */
    void setType(std::string _type);

    /**
     * Setter function
     * @param _type The new value of the simpType and derType member variables,
     *              given in string format
     * @sa simpleType and derType
     */
    void setType(jsonDataType _type);

    /**
     * Setter function
     * @param _type The new value of the derType member variable,
     *              given in string format
     * @sa derType
     */
    void setDerType(std::string _type);

    /**
     * Setter function for variables associated with the unit quality
     *
     * @param _units    The new value of the unit member variable
     * @param _scaleMin The new value of the scaleMinimum member variable
     * @param _scaleMax The new value of the scaleMaximum member variable
     *
     * @sa units, scaleMinimum and scaleMaximum
     */
    void setUnits(std::string _units, float _scaleMin = NAN,
            float _scaleMax = NAN);

    /**
     * Setter function for variables of the readable and writable qualities
     *
     * @param _readable The new value of the readable member variable
     * @param _writable The new value of the writable member variable
     *
     * @sa readable and writable
     */
    void setReadWrite(bool _readable, bool _writable);

    /**
     * Setter function for variables representing the readable quality
     * @param _readable The new value of the readable member variable
     * @sa readable
     */
    void setReadable(bool _readable);

    /**
     * Setter function for variables representing the writable quality
     * @param _writable The new value of the writable member variable
     * @sa writable
     */
    void setWritable(bool _writable);

    /**
     * Setter function for variables of the observable and nullable qualities
     *
     * @param _observable The new value of the observable member variable
     * @param _nullable   The new value of the nullable member variable
     *
     * @sa observable and nullable
     */
    void setObserveNull(bool _observable, bool _nullable);

    /**
     * Setter function for the variable representing the format member variable
     * @param _format The new value of the format member variable
     * @sa format
     */
    void setFormat(jsonSchemaFormat _format);

    /**
     * Setter function for the variable representing the sdfType quality
     * @param _subtype The new value of the subtype member variable
     * @sa subtype
     */
    void setSubtype(sdfSubtype _subtype);

    /**
     * Setter function for the uniqueItems member
     * variable. Sets uniqueItemsDefined to true.
     * @param unique The new value of the uniqueItems member variable
     * @sa uniqueItems and uniqueItemsDefined
     */
    void setUniqueItems(bool unique);

    /**
     * Setter function for the minimum member variable
     * @param min The new value of the minimum member variable
     * @sa minimum
     */
    void setMinimum(double min);

    /**
     * Setter function for the maximum member variable
     * @param max The new value of the maximum member variable
     * @sa maximum
     */
    void setMaximum(double max);

    /**
     * Setter function for the minInt member variable. Sets minIntSet to true.
     * @param min The new value of the minInt member variable.
     * @sa minInt and minIntSet
     */
    void setMinInt(int64_t min);

    /**
     * Setter function for the maxInt member variable. Sets maxIntSet to true.
     * @param max The new value of the maxInt member variable.
     * @sa maxInt and maxIntSet
     */
    void setMaxInt(uint64_t max);

    /**
     * Setter function to delete the minInt member variable.
     * Sets minIntSet to false.
     * @sa minInt and minIntSet
     */
    void eraseMinInt();

    /**
     * Setter function to delete the maxInt member variable.
     * Sets maxIntSet to false.
     * @sa maxInt and maxIntSet
     */
    void eraseMaxInt();

    /**
     * Setter function for the multipleOf member variable
     * @param mult The new value of the multipleOf member variable
     * @sa multipleOf
     */
    void setMultipleOf(float mult);

    /**
     * Setter function for the maxItems member variable
     * @param maxItems The new value of the maxItems member variable
     * @sa maxItems
     */
    void setMaxItems(float maxItems);

    /**
     * Setter function for the minItems member variable
     * @param minItems The new value of the minItems member variable
     * @sa minItems
     */
    void setMinItems(float minItems);

    /**
     * Setter function for the minLength member variable
     * @param min The new value of the minLength member variable
     * @sa minLength
     */
    void setMinLength(float min);

    /**
     * Setter function for the maxLength member variable
     * @param max The new value of the maxLength member variable
     * @sa maxLength
     */
    void setMaxLength(float max);

    /**
     * Setter function for the maximum pattern variable
     * @param pattern The new value of the pattern member variable
     * @sa pattern
     */
    void setPattern(std::string pattern);

    /**
     * Setter function for the constantBool member variable. Sets
     * constBoolDefined to true.
     * @param constantBool The new value of the constantBool member variable
     * @sa constantBool, constBoolDefined and constDefined
     */
    void setConstantBool(bool constantBool);

    /**
     * Setter function for the constantInt member variable. Sets
     * constIntDefined and constDefined to true.
     * @param constantInt The new value of the constantInt member variable
     * @sa constantInt, constIntDefined and constDefined
     */
    void setConstantInt(int64_t constantInt);

    /**
     * Setter function for the constantNumber member variable. Sets
     * constDefined to true.
     * @param constantNumber The new value of the constantNumber member variable
     * @sa constantNumber and constDefined
     */
    void setConstantNumber(float constantNumber);

    /**
     * Setter function for the constantString member variable. Sets
     * constDefined to true.
     * @param constantString The new value of the constantString member variable
     * @sa constantString and constDefined
     */
    void setConstantString(std::string constantString);

    /**
     * Setter function for the defaultBool member variable. Sets
     * defaultBoolDefined and  defaultDefined to true.
     * @param defaultBool The new value of the defaultBool member variable
     * @sa defaultBool, defaultBoolDefined and defaultDefined
     */
    void setDefaultBool(bool defaultBool);

    /**
     * Setter function for the defaultInt member variable. Sets
     * defaultIntDefined and  defaultDefined to true.
     * @param defaultInt The new value of the defaultInt member variable
     * @sa defaultInt, defaultIntDefined and defaultDefined
     */
    void setDefaultInt(int64_t defaultInt);

    /**
     * Setter function for the defaultNumber member variable.
     * Sets defaultDefined to true.
     * @param defaultNumber The new value of the defaultNumber member variable
     * @sa defaultNumber and defaultDefined
     */
    void setDefaultNumber(float defaultNumber);

    /**
     * Setter function for the defaultString member variable.
     * Sets defaultDefined to true.
     * @param defaultString The new value of the defaultString member variable
     * @sa defaultString and defaultDefined
     */
    void setDefaultString(std::string defaultString);

    /**
     * (Overloaded) setter function for the constantBoolArray member variable.
     * Sets constDefined to true.
     * @param constantArray The new value of the constantBoolArray member
     *                      variable
     * @sa constantBoolArray and constDefined
     */
    void setConstantArray(std::vector<bool> constantArray);

    /**
     * (Overloaded) setter function for the defaultBoolArray member variable.
     * Sets defaultDefined to true.
     * @param defaultArray The new value of the defaultBoolArray member
     *                     variable
     * @sa defaultBoolArray defaultDefined
     */
    void setDefaultArray(std::vector<bool> defaultArray);

    /**
     * (Overloaded) setter function for the constantIntArray member variable.
     * Sets constDefined to true.
     * @param constantArray The new value of the constantIntArray member
     *                      variable
     * @sa constantIntArray and constDefined
     */
    void setConstantArray(std::vector<int64_t> constantArray);

    /**
     * (Overloaded) setter function for the defaultIntArray member variable.
     * Sets defaultDefined to true.
     * @param defaultArray The new value of the defaultIntArray member
     *                     variable
     * @sa defaultIntArray and defaultDefined
     */
    void setDefaultArray(std::vector<int64_t> defaultArray);

    /**
     * (Overloaded) setter function for the constantNumberArray member variable.
     * Sets constDefined to true.
     * @param constantArray The new value of the constantNumberArray member
     *                      variable
     * @sa constantNumberArray and constDefined
     */
    void setConstantArray(std::vector<float> constantArray);

    /**
     * (Overloaded) setter function for the defaultNumberArray member variable.
     * Sets defaultDefined to true.
     * @param defaultArray The new value of the defaultNumberArray member
     *                     variable
     * @sa defaultNumberArray and defaultDefined
     */

    void setDefaultArray(std::vector<float> defaultArray);

    /**
     * (Overloaded) setter function for the constantStringArray member variable.
     * Sets constDefined to true.
     * @param constantArray The new value of the constantStringArray member
     *                      variable
     * @sa constantStringArray and constDefined
     */

    void setConstantArray(std::vector<std::string> constantArray);

    /**
     * (Overloaded) setter function for the defaultStringArray member variable.
     * Sets defaultDefined to true.
     * @param defaultArray The new value of the defaultStringArray member
     *                     variable
     * @sa defaultStringArray and defaultDefined
     */

    void setDefaultArray(std::vector<std::string> defaultArray);

    //void setConstantObject(sdfData *object);
    //void setDefaultObject(sdfData *object);
    /**
     * Setter function for the enumString member variable
     * @param enm The new value of the enumString member variable.
     * @sa enumString
     */
    void setEnumString(std::vector<std::string> enm);

    /**
     * Setter function for the item_constr member variable
     * @param constr The new value of the item_constr member variable.
     * @sa item_constr
     */
    void setItemConstr(sdfData* constr);

    /**
     * Setter function for the parent member variable
     * @param parentCommon The new value of the parent member variable.
     * @sa parent
     */
    void setParentCommon(sdfCommon *parentCommon);

    /**
     * Setter function for the sdfChoice member variable
     * @param choices The new value of the sdfChoice member variable.
     * @sa sdfChoice
     */
    void setChoice(std::vector<sdfData*> choices);

    /**
     * Add an element to the sdfChoice member vector
     * @param choice The value to add to the sdfChoice member vector.
     * @sa sdfChoice
     */
    void addChoice(sdfData *choice);

    /**
     * Add an element to the objectProperties member vector
     * @param property The value to add to the objectProperties member vector.
     * @sa objectProperties
     */
    void addObjectProperty(sdfData *property);

    /**
     * Setter function for the objectProperties member variable
     * @param properties The new value of the objectProperties member variable.
     * @sa objectProperties
     */
    void setObjectProperties(std::vector<sdfData*> properties);

    /**
     * Add an element to the required member vector
     * @param propertyName The value to add to the required member vector.
     * @sa required
     */
    void addRequiredObjectProperty(std::string propertyName);

    // getters for member variables
    /**
     * Getter function for the readable member variable
     * @return The value of the readable member variable.
     *
     * @sa readable
     */
    bool getReadable();

    /**
     * Getter function for the writable member variable
     * @return The value of the writable member variable.
     *
     * @sa writable
     */
    bool getWritable();

    /**
     * Getter function for the nullable member variable
     *
     * @return The value of the nullable member variable.
     *
     * @sa nullable
     */
    bool getNullable();

    /**
     * Getter function for the observable member variable
     * @return The value of the observable member variable.
     *
     * @sa observable
     */
    bool getObservable();

    /**
     * Getter function for the member variable readableDefined
     *
     * @return The value of the member variable readableDefined.
     *
     * @sa readableDefined
     */
    bool getReadableDefined() const;

    /**
     * Getter function for the member variable writableDefined
     *
     * @return The value of the member variable writableDefined.
     *
     * @sa writableDefined
     */
    bool getWritableDefined() const;

    /**
     * Getter function for the member variable nullableDefined
     *
     * @return The value of the member variable nullableDefined.
     *
     * @sa nullableDefined
     */
    bool getNullableDefined() const;

    /**
     * Getter function for the member variable observableDefined
     *
     * @return The value of the member variable observableDefined.
     *
     * @sa observableDefined
     */
    bool getObservableDefined() const;

    /**
     * Getter function for the member variable constantBool
     *
     * @return The value of the member variable constantBool.
     *
     * @sa constantBool
     */
    bool getConstantBool();

    /**
     * Getter function for the member variable constantInt
     *
     * @return The value of the member variable constantInt.
     *
     * @sa constantInt
     */
    int64_t getConstantInt();

    /**
     * Getter function for the member variable constantNumber
     *
     * @return The value of the member variable constantNumber.
     *
     * @sa constantNumber
     */
    float getConstantNumber();

    /**
     * Getter function for the member variable constantString
     *
     * @return The value of the member variable constantString.
     *
     * @sa constantString
     */
    std::string getConstantString();

    //sdfData* getConstantObject() const;

    /**
     * Returns a string value corresponding to the value of one of the member
     * variables representing a constant value. Which member is chosen is
     * decided based on which one is defined.
     *
     * @return The value of one of the member variables representing a
     * constant value as a string.
     *
     * @sa constantString, constantInt, constantNumber, constantBool
     */
    std::string getConstantAsString();

    /**
     * Getter function for the member variable contentFormat
     *
     * @return The value of the member variable contentFormat.
     *
     * @sa contentFormat
     */
    std::string getContentFormat();

    /**
     * Getter function for the member variable defaultBool
     *
     * @return The value of the member variable defaultBool.
     *
     * @sa defaultBool
     */
    bool getDefaultBool();

    /**
     * Getter function for the member variable defaultInt
     *
     * @return The value of the member variable defaultInt.
     *
     * @sa defaultInt
     */
    int64_t getDefaultInt();

    /**
     * Getter function for the member variable defaultNumber
     *
     * @return The value of the member variable defaultNumber.
     *
     * @sa defaultNumber
     */
    float getDefaultNumber();

    /**
     * Getter function for the member variable defaultString
     *
     * @return The value of the member variable defaultString.
     *
     * @sa defaultString
     */
    std::string getDefaultString();
    //sdfData* getDefaultObject() const;

    //sdfData* getConstantObject() const;

    /**
     * Returns a string value corresponding to the value of one of the member
     * variables representing a default value. Which member is chosen is
     * decided based on which one is defined.
     *
     * @return The value of one of the member variables representing a
     * default value in the form of a string.
     *
     * @sa defaultString, defaultInt, defaultNumber, defaultBool
     */
    std::string getDefaultAsString();
    std::vector<std::string> getDefaultArrayAsStringVector();

    /**
     * Getter function for the member variable defaultDefined
     *
     * @return The value of the member variable defaultDefined.
     *
     * @sa defaultDefined
     */
    bool getDefaultDefined() const;

    /**
     * Getter function for the member variable constDefined
     * @return The value of the member variable constDefined.
     *
     * @sa constDefined
     */
    bool getConstantDefined() const;

    /**
     * Getter function for the member variable defaultIntDefined
     *
     *
     * @return The value of the member variable defaultIntDefined.
     *
     * @sa defaultIntDefined
     */
    bool getDefaultIntDefined() const;

    /**
     * Getter function for the member variable constIntDefined
     *
     * @return The value of the member variable constIntDefined.
     *
     * @sa constIntDefined
     */
    bool getConstantIntDefined() const;

    /**
     * Getter function for the member variable defaultBoolDefined
     *
     * @return The value of the member variable defaultBoolDefined.
     *
     * @sa defaultBoolDefined
     */
    bool getDefaultBoolDefined() const;

    /**
     * Getter function for the member variable constBoolDefined
     *
     * @return The value of the member variable constBoolDefined.
     *
     * @sa constBoolDefined
     */
    bool getConstantBoolDefined() const;

    /**
     * Getter function for the member variable enumString
     *
     * @return The value of the member variable enumString.
     *
     * @sa enumString
     */
    std::vector<std::string> getEnumString();

    /**
     * Getter function for the member variable exclusiveMaximum_bool
     *
     * @return The value of the member variable exclusiveMaximum_bool.
     *
     * @sa exclusiveMaximum_bool
     */
    bool isExclusiveMaximumBool();

    /**
     * Getter function for the member variable exclusiveMaximum_number
     *
     * @return The value of the member variable exclusiveMaximum_number.
     *
     * @sa exclusiveMaximum_number
     */
    float getExclusiveMaximumNumber();

    /**
     * Getter function for the member variable exclusiveMinimum_bool
     *
     * @return The value of the member variable exclusiveMinimum_bool.
     *
     * @sa exclusiveMinimum_bool
     */
    bool isExclusiveMinimumBool();

    /**
     * Getter function for the member variable exclusiveMinimum_number
     *
     * @return The value of the member variable exclusiveMinimum_number.
     *
     * @sa exclusiveMinimum_number
     */
    float getExclusiveMinimumNumber();

    /**
     * Getter function for the member variable format
     *
     * @return The value of the member variable format.
     *
     * @sa format
     */
    jsonSchemaFormat getFormat();

    /**
     * Getter function for the member variable maximum
     *
     * @return The value of the member variable maximum.
     *
     * @sa maximum
     */
    float getMaximum();

    /**
     * Getter function for the member variable maxItems
     *
     * @return The value of the member variable maxItems.
     *
     * @sa maxItems
     */
    float getMaxItems();

    /**
     * Returns the value of the member variable maxItems or, if it exists,
     * the maxItems value of the referenced sdfData definition.
     *
     * @return The maxItems value of
     *         the referenced sdfData definition if it exists,
     *         the value of the own member variable maxItems otherwise.
     *
     * @sa maxItems
     */
    float getMaxItemsOfRef();

    /**
     * Getter function for the member variable maxLength
     *
     * @return The value of the member variable maxLength.
     *
     * @sa maxLength
     */
    float getMaxLength();

    /**
     * Getter function for the member variable minimum
     *
     * @return The value of the member variable minimum.
     *
     * @sa minimum
     */
    float getMinimum();

    /**
     * Getter function for the member variable minItems
     *
     * @return The value of the member variable minItems.
     *
     * @sa minItems
     */
    float getMinItems();

    /**
     * Returns the value of the member variable minItems or, if it exists,
     * the minItems value of the referenced sdfData definition.
     *
     * @return The minItems value of
     *         the referenced sdfData definition if it exists,
     *         the value of the own member variable minItems otherwise.
     *
     * @sa minItems
     */
    float getMinItemsOfRef();

    /**
     * Getter function for the member variable minLength
     *
     * @return The value of the member variable minLength.
     *
     * @sa minLength
     */
    float getMinLength();

    /**
     * Getter function for the member variable minInt
     *
     * @return The value of the member variable minInt.
     *
     * @sa minInt
     */
    int64_t getMinInt() const;

    /**
     * Getter function for the member variable maxInt
     *
     * @return The value of the member variable maxInt.
     *
     * @sa maxInt
     */
    uint64_t getMaxInt() const;

    /**
     * Getter function for the member variable minIntSet
     *
     * @return The value of the member variable minIntSet.
     *
     * @sa minIntSet
     */
    bool getMinIntSet() const;

    /**
     * Getter function for the member variable maxIntSet
     *
     * @return The value of the member variable maxIntSet.
     *
     * @sa maxIntSet
     */
    bool getMaxIntSet() const;

    /**
     * Getter function for the member variable multipleOf
     *
     * @return The value of the member variable multipleOf.
     *
     * @sa multipleOf
     */
    float getMultipleOf();

    /**
     * Getter function for the member variable pattern
     *
     * @return The value of the member variable pattern.
     *
     * @sa pattern
     */
    std::string getPattern();

    /**
     * Getter function for the member variable scaleMaximum
     *
     * @return The value of the member variable scaleMaximum.
     *
     * @sa scaleMaximum
     */
    float getScaleMaximum();

    /**
     * Getter function for the member variable scaleMinimum
     *
     * @return The value of the member variable scaleMinimum.
     *
     * @sa scaleMinimum
     */
    float getScaleMinimum();

    /**
     * Getter function for the member variable subtype (represents the sdfType
     * quality).
     *
     * @return The value of the member variable subtype.
     *
     * @sa subtype
     */
    sdfSubtype getSubtype();

    /**
     * Getter function for the member variable simpleType, or, if it exists,
     * the simpleType value of the referenced sdfData definition, or, if
     * there is an sdfChoice where all choices have the same simpleType value,
     * that simpleType.
     *
     * @return The value of the member variable simpleType of either this
     *         sdfData object, or of the referenced sdfData object
     *         (if it exists)
     *         or of the sdfChoice (if it exists and if all choices have the
     *         same simpleType value).
     *
     * @sa simpleType
     */
    jsonDataType getSimpType();

    /**
     * Getter function for the member variable derType, or, if it exists,
     * the derType value of the referenced sdfData definition.
     *
     * @return The value of the member variable derType of either this
     *         sdfData object or of the referenced sdfData object
     *         (if it exists).
     *
     * @sa derType
     */
    std::string getType();

    /**
     * Getter function for the member variable uniqueItems.
     *
     * @return The value of the member variable uniqueItems.
     *
     * @sa uniqueItems
     */
    bool getUniqueItems();

    /**
     * Getter function for the member variable uniqueItemsDefined.
     *
     * @return The value of the member variable uniqueItemsDefined.
     *
     * @sa uniqueItemsDefined
     */
    bool getUniqueItemsDefined() const;

    /**
     * Getter function for the member variable units if it is not the empty
     * string or the subtype (sdfType) value or the format value, depending
     * on which is defined.
     *
     * @return The value of the member variable units, subtype or format,
     *         depending on which one is defined.
     *
     * @sa units, subtype and format
     */
    std::string getUnits();

    /**
     * Getter function for the member variable units as a C-string.
     *
     * @return The value of the member variable units as a C-string.
     *
     * @sa units as a C-string
     */
    const char* getUnitsAsArray() const;

    /**
     * Getter function for the member variable constantBoolArray.
     *
     * @return The value of the member variable constantBoolArray.
     *
     * @sa constantBoolArray
     */
    std::vector<bool> getConstantBoolArray() const;

    /**
     * Getter function for the member variable defaultBoolArray.
     *
     * @return The value of the member variable defaultBoolArray.
     *
     * @sa defaultBoolArray
     */
    std::vector<bool> getDefaultBoolArray() const;

    /**
     * Getter function for the member variable constantIntArray.
     *
     * @return The value of the member variable constantIntArray.
     *
     * @sa constantIntArray
     */
    std::vector<int64_t> getConstantIntArray() const;

    /**
     * Getter function for the member variable defaultIntArray.
     *
     * @return The value of the member variable defaultIntArray.
     *
     * @sa defaultIntArray
     */
    std::vector<int64_t> getDefaultIntArray() const;

    /**
     * Getter function for the member variable constantNumberArray.
     *
     * @return The value of the member variable constantNumberArray.
     *
     * @sa constantNumberArray
     */
    std::vector<float> getConstantNumberArray() const;

    /**
     * Getter function for the member variable defaultNumberArray.
     *
     * @return The value of the member variable defaultNumberArray.
     *
     * @sa defaultNumberArray
     */
    std::vector<float> getDefaultNumberArray() const;

    /**
     * Getter function for the member variable constantStringArray.
     *
     * @return The value of the member variable constantStringArray.
     *
     * @sa constantStringArray
     */
    std::vector<std::string> getConstantStringArray() const;

    /**
     * Getter function for the member variable defaultStringArray.
     *
     * @return The value of the member variable defaultStringArray.
     *
     * @sa defaultStringArray
     */
    std::vector<std::string> getDefaultStringArray() const;

    /**
     * Getter function for the member variable parent.
     *
     * @return The value of the member variable parent.
     *
     * @sa parent
     */
    sdfCommon* getParentCommon() const;

    /**
     * Getter function for the member variable parent.
     *
     * @return The value of the member variable parent.
     *
     * @sa parent
     */
    sdfCommon* getParent() const;

    /**
     * Getter function for the member variable item_constr.
     *
     * @return The value of the member variable item_constr.
     *
     * @sa item_constr
     */
    sdfData* getItemConstr() const;

    /**
     * Returns the value of the member variable item_constr or, if it exists,
     * the item_constr value of the referenced sdfData definition.
     *
     * @return The item_constr value of
     *         the referenced sdfData definition if it exists,
     *         the value of the own member variable item_constr otherwise.
     *
     * @sa item_constr
     */
    sdfData* getItemConstrOfRefs() const;

    /**
     * Return whether or not this sdfData object is pointed at by the
     * item_constr member variable of another sdfData object.
     *
     * @return True if another sdfData object's item_constr member variable
     *         points to this sdfData object, false otherwise.
     *
     * @sa item_constr
     */
    bool isItemConstr() const;

    /**
     * Return whether or not this sdfData object is listed in the
     * objectProperties member variable of another sdfData object.
     *
     * @return True if another sdfData object's objectProperties member variable
     *         contains a pointer to this sdfData object, false otherwise.
     *
     * @sa objectProperties
     */
    bool isObjectProp() const;

    /**
     * Getter function for the member variable sdfChoice.
     *
     * @return The value of the member variable sdfChoice.
     *
     * @sa sdfChoice
     */
    std::vector<sdfData*> getChoice() const;

    /**
     * Getter function for the member variable objectProperties.
     *
     * @return The value of the member variable objectProperties.
     *
     * @sa objectProperties
     */
    std::vector<sdfData*> getObjectProperties() const;

    /**
     * Return whether or not this sdfData object is listed in the
     * objectProperties member variable of another sdfData object.
     *
     * @return True if another sdfData object's objectProperties member variable
     *         contains a pointer to this sdfData object, false otherwise.
     *
     * @sa objectProperties
     */
    std::vector<sdfData*> getObjectPropertiesOfRefs() const;

    /**
     * Getter function for the member variable requiredObjectProperties.
     *
     * @return The value of the member variable requiredObjectProperties.
     *
     * @sa requiredObjectProperties
     */
    std::vector<std::string> getRequiredObjectProperties() const;

    /**
     * Returns a pointer to this sdfData object
     *
     * @return this
     */
    sdfData* getThisAsSdfData();
    // parsing
    /**
     * Function catering to the SDF/YANG converter by providing means to
     * transfer information from a lys_node's default field into the
     * corresponding default member variable of this sdfData object.
     *
     * @param value The C-string to transfer the value from
     */
    void parseDefault(const char *value);

    /**
     * Function catering to the SDF/YANG converter by providing means to
     * transfer information from a lys_node's default array into the
     * corresponding default array member variable of this sdfData object.
     *
     * @param node The lys_node that contains the default array
     */
    void parseDefaultArray(lys_node_leaflist *node);

    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);

    /**
     * Transfer the information from this sdfData object into a JSON object.
     *
     * @param prefix JSON object to add to (prefix does not mean the same prefix
     *               as in the namespace prefix)
     *
     * @return The completed JSON object
     */
    nlohmann::json dataToJson(nlohmann::json prefix);

    /**
     * Transfer the information from a given JSON object into this
     * sdfData object.
     *
     * @param input The input JSON object
     */
    sdfData* jsonToData(nlohmann::json input);

private:
    // TODO: use a C union for constant and default values, maximum, minimum?
    /**
     * True if a constant value is defined. This is necessary because
     * some types of variables cannot be set to NULL.
     * @sa getConstantDefined()
     */
    bool constDefined;

    /**
     * True if a default value is defined. This is necessary because
     * some types of variables cannot be set to NULL.
     * @sa getDefaultDefined()
     */
    bool defaultDefined;

    /**
     * Represents the type quality
     * @sa setSimpType() and setType()
     */
    jsonDataType simpleType;

    /**
     * Represents the type quality as a string
     * @sa setDerType() and setType()
     */
    std::string derType;

    /**
     * Represents the const quality for number values
     * @sa setConstantNumber() and getConstantNumber()
     */
    float constantNumber;

    /**
     * Represents the default quality for number values
     * @sa setDefaultNumber() and getDefaultNumber()
     */
    float defaultNumber;

    // only fill for type string
    /**
     * Represents the enum quality
     * @sa setEnumString() and getEnumString()
     */
    std::vector<std::string> enumString;

    /**
     * Represents the const quality if type is string
     * @sa setConstantString() and getConstantString()
     */
    std::string constantString;

    /**
     * Represents the default quality if type is string
     * @sa setDefaultString() and getDefaultString()
     */
    std::string defaultString;

    // only fill for type boolean
    /**
     * Represents the const quality if type is boolean
     * @sa setConstantBool() and getConstantBool()
      */
    bool constantBool;

    /**
     * Represents the default quality if type is boolean
     * @sa setDefaultBool() and getDefaultBool()
      */
    bool defaultBool;

    /**
     * True if a constant boolean value is defined. This is necessary because
     * boolean variables cannot be set to NULL.
     * @sa constantBool and getConstantBoolDefined()
     */
    bool constBoolDefined;

    /**
     * True if a default boolean value is defined. This is necessary because
     * boolean variables cannot be set to NULL.
     * @sa defaultBool and getDefaultBoolDefined()
     */
    bool defaultBoolDefined;

    // only fill for type integer
    /**
     * Represents the const quality if type is integer
     * @sa setConstantInt() and getConstantInt()
     */
    int64_t constantInt;

    /**
     * Represents the default quality if type is integer
     * @sa setDefaultInt() and getDefaultInt()
      */
    int64_t defaultInt;

    /**
     * True if a constant integer value is defined. This is necessary because
     * integer variables cannot be set to NULL.
     * @sa constantInt and getConstantIntDefined()
     */
    bool constIntDefined;

    /**
     * True if a default integer value is defined. This is necessary because
     * integer variables cannot be set to NULL.
     * @sa defaultInt and getDefaultIntDefined()
     */
    bool defaultIntDefined;

    // only fill for type array
    /**
     * Represents the const quality if type is array with boolean-typed
     * items
     * @sa setConstantArray() and getConstantBoolArray()
     */
    std::vector<bool> constantBoolArray;

    /**
     * Represents the default quality if type is array with boolean-typed
     * items
     * @sa setDefaultArray() and getDefaultBoolArray()
     */
    std::vector<bool> defaultBoolArray;

    /**
     * Represents the const quality if type is array with integer-typed
     * items
     * @sa setConstantArray() and getConstantIntArray()
     */
    std::vector<int64_t> constantIntArray;

    /**
     * Represents the default quality if type is array with integer-typed
     * items
     * @sa setDefaultArray() and getDefaultIntArray()
     */
    std::vector<int64_t> defaultIntArray;

    /**
     * Represents the const quality if type is array with number-typed
     * items
     * @sa setConstantArray() and getConstantNumberArray()
     */
    std::vector<float> constantNumberArray;

    /**
     * Represents the default quality if type is array with number-typed
     * items
     * @sa setDefaultArray() and getDefaultNumberArray()
     */
    std::vector<float> defaultNumberArray;

    /**
     * Represents the const quality if type is array with string-typed
     * items
     * @sa setConstantArray() and getConstantStringArray()
     */
    std::vector<std::string> constantStringArray;

    /**
     * Represents the default quality if type is array with string-typed
     * items
     * @sa setDefaultArray() and getDefaultStringArray()
     */
    std::vector<std::string> defaultStringArray;

    //sdfData *constantObject;
    //sdfData *defaultObject;

    /**
     * Represents the minimum quality if type is number
     * @sa setMinimum() and getMinimum()
     */
    double minimum;

    /**
     * Represents the maximum quality if type is number
     * @sa setMaximum() and getMaximum()
     */
    double maximum;

    /**
     * Represents the minimum quality if type is integer
     * @sa setMinInt(), getMinInt() and eraseMinInt()
     */
    int64_t minInt;

    /**
     * Represents the maximum quality if type is integer
     * @sa setMaxInt(), getMaxInt() and eraseMinInt()
     */
    uint64_t maxInt;

    /**
     * True if a minimum integer value is defined. This is necessary because
     * integer variables cannot be set to NULL.
     * @sa minInt and getMinIntSet()
     */
    bool minIntSet;

    /**
     * True if a maximum integer value is defined. This is necessary because
     * integer variables cannot be set to NULL.
     * @sa maxInt and getMaxIntSet()
     */
    bool maxIntSet;

    // TODO: number > exclusiveMin vs >=
    /**
     * Represents the exclusiveMinimum quality of type number
     * @sa getExclusiveMinimumNumber()
     */
    float exclusiveMinimum_number;

    /**
     * Represents the exclusiveMaximum quality of type number
     * @sa getExclusiveMaximumNumber()
     */
    float exclusiveMaximum_number;

    /**
     * Represents the exclusiveMinimum quality of type boolean (implicating
     * that the minimum variable states an exclusive minimum)
     * @sa isExclusiveMinimumBool()
     */
    bool exclusiveMinimum_bool;

    /**
     * Represents the exclusiveMaximum quality of type boolean (implicating
     * that the maximum variable states an exclusive maximum)
     * @sa isExclusiveMaximumBool()
     */
    bool exclusiveMaximum_bool;

    /**
     * Represents the multipleOf quality
     * @sa setMultipleOf() and getMultipleOf()
     */
    float multipleOf;

    /**
     * Represents the minLength quality
     * @sa setMinLength() and getMinLength()
     */
    float minLength;

    /**
     * Represents the maxLength quality
     * @sa setMaxLength() and getMaxLength()
     */
    float maxLength;

    /**
     * Represents the pattern quality
     * @sa setPattern() and getPattern()
     */
    std::string pattern;

    /**
     * Represents the format quality
     * @sa setFormat() and getFormat()
     */
    jsonSchemaFormat format;

    /**
     * Represents the minItems quality
     * @sa setMinItems() and getMinItems()
     */
    float minItems;

    /**
     * Represents the maxItems quality
     * @sa setMaxItems() and getMaxItems()
     */
    float maxItems;

    /**
     * Represents the uniqueItems quality
     * @sa setUniqueItems() and getUniqueItems()
     */
    bool uniqueItems;

    /**
     * True if the uniqueItems value is defined. This is necessary because
     * boolean variables cannot be set to NULL.
     * @sa uniqueItems and getUniqueItemsDefined()
     */
    bool uniqueItemsDefined;

    /**
     * Represents the items quality, thus the constraints on the array items
     * (represented by another sdfData object)
     * @sa setItemConstr() and getItemConstr()
     */
    sdfData *item_constr;

    // SDF-defined data qualities
    /**
     * Represents the unit quality
     * @sa setUnits() and getUnits()
     */
    std::string units;

    /**
     * Represents the scaleMinimum quality
     * @sa getScaleMinimum()
     */
    float scaleMinimum;

    /**
     * Represents the scaleMaximum quality
     * @sa getScaleMaximum()
     */
    float scaleMaximum;

    /**
     * Represents the readable quality
     * @sa setReadable and getReadable()
      */
    bool readable;

    /**
     * Represents the writable quality
     * @sa setWritable and getWritable()
      */
    bool writable;

    /**
     * Represents the observable quality
     * @sa setObserveNull() and getObservable()
     */
    bool observable;

    /**
     * Represents the nullable quality
     * @sa setObserveNull() and getNullable()
     */
    bool nullable;

    /**
     * True if the readable value is defined. This is necessary because
     * boolean variables cannot be set to NULL.
     * @sa readable and getReadableDefined()
     */
    bool readableDefined;

    /**
     * True if the writable value is defined. This is necessary because
     * boolean variables cannot be set to NULL.
     * @sa writable and getWritableDefined()
     */
    bool writableDefined;

    /**
     * True if the observable value is defined. This is necessary because
     * boolean variables cannot be set to NULL.
     * @sa observable and getObservableDefined()
     */
    bool observableDefined;

    /**
     * True if the nullable value is defined. This is necessary because
     * boolean variables cannot be set to NULL.
     * @sa nullable and getNullableDefined()
     */
    bool nullableDefined;

    /**
     * Represents the contentFormat quality
     * @sa getContentFormat()
     */
    std::string contentFormat;

    // TODO: rename sdfSubtype to sdfType
    /**
     * Represents the sdfType quality
     * @sa setSubtype() and getSubtype()
     */
    sdfSubtype subtype;

    /**
     * Represents the properties quality
     * @sa setObjectProperties(), getObjectProperties and addObjectProperties()
     */
    std::vector<sdfData*> objectProperties;

    /**
     * Represents the required quality
     * @sa getRequiredObjectProperties() and addRequiredObjectProperty()
     */
    std::vector<std::string> requiredObjectProperties;

    /**
     * Represents the sdfChoice quality
     * @sa getChoice(), setChoice() and addChoice()
     */
    std::vector<sdfData*> sdfChoice;

    /**
     * Holds a pointer to the superordinate sdfCommon object
     * @sa setParentCommon()
     */
    sdfCommon *parent;
};

/**
 * This class represents sdfElement definitions and inherits from the
 * sdfObjectElement class. SdfEvents can appear
 * inside sdfObject definitions, thus the this class inherits from the
 * sdfObjectElement class.
 */
class sdfEvent : virtual public sdfObjectElement
{
public:
    /**
     * The sdfEvent constructor
     * @param _name         The name
     * @param _description  The description
     * @param _reference    sdfRef quality: a pointer to another sdfCommon
     *                      referenced through
     *                      the sdfRef quality
     * @param _required     sdfRequired: a vector of pointers to subordinate
     *                      sdfCommons that are marked as required through the
     *                      sdfRequired quality
     * @param _parentObject A pointer to the superordinate sdfObject
     * @param _outputData   A pointer to the sdfData object defining the output
     * @param _datatypes    A vector with pointers to the datatypes
     */
    sdfEvent(std::string _name = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            sdfData* _outputData = NULL,
            std::vector<sdfData*> _datatypes = {});

    /**
     * The sdfEvent destructor.
     *
     * A destructor is needed to free the pointers to the output data and
     * datatypes.
     */
    virtual ~sdfEvent();

    // setters
    /**
     * Setter function for the outputData member variable
     * @param outputData The new value of the outputData member variable.
     * @sa outputData
     */
    void setOutputData(sdfData *outputData);

    /**
     * Add an sdfData object pointer to the for the datatypes member variable
     *
     * @param datatype The new value to add to the datatypes member variable.
     *
     * @sa datatypes
     */
    void addDatatype(sdfData *datatype);

    // getters
    /**
     * Getter function for the member variable datatypes.
     *
     * @return The value of the member variable datatypes.
     *
     * @sa datatypes
     */
    std::vector<sdfData*> getDatatypes();

    /**
     * Getter function for the member variable outputData.
     *
     * @return The value of the member variable outputData.
     *
     * @sa outputData
     */
    sdfData* getOutputData();

    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);

   /**
    * Transfer the information from this sdfEvent object into a
    * JSON object
    *
    * @param prefix JSON object to add to (prefix does not mean the same prefix
    *               as in the namespace prefix)
    *
    * @return The completed JSON object
    */
    nlohmann::json eventToJson(nlohmann::json prefix);

    /**
     * Transfer the information from a given JSON object into this
     * sdfEvent object.
     *
     * @param input The input JSON object
     */
    sdfEvent* jsonToEvent(nlohmann::json input);
private:
    /**
     * Member variable representing the sdfOutputData quality of sdfEvent
     * definitions.
     * @sa getOutputData() and setOutputData()
     */
    sdfData *outputData;
    /**
     * Member variable representing the sdfData quality of sdfEvent
     * definitions.
     * @sa getDatatypes() and addDatatype()
     */
    std::vector<sdfData*> datatypes;
};

/**
 * This class can be used to represent sdfActions. Since sdfActions can appear
 * inside sdfObject definitions, the sdfAction class inherits from the
 * sdfObjectElement class.
 */
class sdfAction : virtual public sdfObjectElement
{
public:
    /**
     * The sdfAction constructor
     * @param _name               The name
     * @param _description        The description
     * @param _reference          sdfRef quality: a pointer to another sdfCommon
     *                            referenced through the sdfRef quality
     * @param _required           sdfRequired: a vector of pointers to
     *                            subordinate sdfCommons that are marked as
     *                            required through the sdfRequired quality
     * @param _parentObject       A pointer to the superordinate sdfObject
     * @param _inputData          A pointer to the sdfData object defining the
     *                            input
     * @param _requiredInputData  A vector with sdfData objects that are
     *                            required input
     * @param _outputData         A pointer to the sdfData object defining the
     *                            output
     * @param _datatypes          A vector with pointers to the datatypes
     */
    sdfAction(std::string _name = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            sdfObject *_parentObject = NULL,
            sdfData* _inputData = NULL,
            std::vector<sdfData*> _requiredInputData = {},
            sdfData* _outputData = NULL,
            std::vector<sdfData*> _datatypes = {});

    /**
     * The sdfAction destructor.
     *
     * A destructor is needed to free the various pointers used in the member
     * variables.
     */
    virtual ~sdfAction();

    // setters
    /**
     * Setter function for the inputData member variable
     *
     * @param inputData The new value of the inputData member variable.
     *
     * @sa inputData
     */
    void setInputData(sdfData* inputData);

    /**
     * Add an sdfData object pointer to the for the requiredInputData member
     * variable
     *
     * @param requiredInputData The new value to add to the requiredInputData
     *                          member variable.
     *
     * @sa requiredInputData
     */
    void addRequiredInputData(sdfData* requiredInputData);

    /**
     * Setter function for the outputData member variable
     *
     * @param outputData The new value of the outputData member variable.
     *
     * @sa outputData
     */
    void setOutputData(sdfData* outputData);

    /**
     * Add an sdfData object pointer to the for the datatypes member variable
     *
     * @param datatype The new value to add to the datatypes member variable.
     *
     * @sa datatypes
     */
    void addDatatype(sdfData *datatype);

    // getters
    /**
     * Getter function for the member variable inputData.
     *
     * @return The value of the member variable inputData.
     *
     * @sa inputData
     */
    sdfData* getInputData();

    /**
     * Getter function for the member variable requiredInputData.
     *
     * @return The value of the member variable requiredInputData.
     *
     * @sa inputData
     */
    std::vector<sdfData*> getRequiredInputData();

    /**
     * Getter function for the member variable outputData.
     *
     * @return The value of the member variable outputData.
     *
     * @sa outputData
     */
    sdfData* getOutputData();

    /**
     * Getter function for the member variable datatypes.
     *
     * @return The value of the member variable datatypes.
     *
     * @sa datatypes
     */
    std::vector<sdfData*> getDatatypes();

    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);

    /**
     * Transfer the information from this sdfAction object into a
     * JSON object
     *
     * @param prefix JSON object to add to (prefix does not mean the same prefix
     *               as in the namespace prefix)
     *
     * @return The completed JSON object
     */
    nlohmann::json actionToJson(nlohmann::json prefix);

    /**
     * Transfer the information from a given JSON object into this
     * sdfAction object.
     *
     * @param input The input JSON object
     */
    sdfAction* jsonToAction(nlohmann::json input);

private:
    /**
     * Member variable representing the sdfInputData quality of sdfAction
     * definitions.
     * @sa getInputData() and setInputData()
     */
    sdfData *inputData;

    /**
     * Member variable representing the sdfRequiredInputData quality of
     * sdfAction definitions (removed in SDF version 1.1).
     * @sa getRequiredInputData() and addRequiredInputData()
     */
    std::vector<sdfData*> requiredInputData; // obsolete

    /**
     * Member variable representing the sdfOutputData quality of sdfAction
     * definitions.
     * @sa getOutputData() and setOutputData()
     */
    sdfData *outputData;

    /**
     * Member variable representing the sdfData quality of sdfAction
     * definitions.
     * @sa getDatatypes() and addDatatype()
     */
    std::vector<sdfData*> datatypes;
};

/**
 * This class represents sdfProperty definitions. Since sdfProperties use the
 * same data qualities as sdfData definitions, this class inherits from the
 * sdfData class. It also inherits from the sdfObjectElement class because it
 * is can be an element of an sdfObject definition.
 */
class sdfProperty : public sdfData, virtual public sdfObjectElement
{
public:
    /**
     * The sdfProperty constructor
     *
     * @param _name         The name
     * @param _description  The description
     * @param _type         The type
     * @param _reference    sdfRef quality: a pointer to another sdfCommon referenced through
     *                      the sdfRef quality
     * @param _required     sdfRequired: a vector of pointers to subordinate
     *                      sdfCommons that are marked as required through the
     *                      sdfRequired quality
     * @param _parentObject A pointer to the superordinate sdfObject
     */
    sdfProperty(std::string _name = "",
                std::string _description = "",
                jsonDataType _type = json_type_undef,
                sdfCommon *_reference = NULL,
                std::vector<sdfCommon*> _required = {},
                sdfObject *_parentObject = NULL);

    /**
     * The sdfProperty copy constructor for casting sdfData objects
     * @param data The address of the sdfData object to copy from
     */
    sdfProperty(sdfData& data);

    // getters
    /**
     * Getter function for the member variable sdfObjectElement::parentObject.
     *
     * @return The value of the member variable sdfObjectElement::parentObject.
     *
     * @sa sdfObjectElement::parentObject
     */
    sdfCommon* getParent() const;

    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);

    /**
     * Transfer the information from this sdfProperty object into a
     * JSON object
     *
     * @param prefix JSON object to add to (prefix does not mean the same prefix
     *               as in the namespace prefix)
     *
     * @return The completed JSON object
     */
    nlohmann::json propertyToJson(nlohmann::json prefix);

    /**
     * Transfer the information from a given JSON object into this
     * sdfProperty object.
     *
     * @param input The input JSON object
     */
    sdfProperty* jsonToProperty(nlohmann::json input);
};

/**
 * This class represents sdfObject definitions and intherits from the sdfCommon
 * class.
 */
class sdfObject : virtual public sdfCommon
{
public:
    /**
     * The sdfObject constructor
     *
     * @param _name        The name quality
     * @param _reference   The sdfRef quality: a pointer to another
     * @param _description The description quality
     *                     referenced sdfCommon object
     * @param _required    The sdfRequired quality: a vector of pointers to
     *                     subordinate sdfCommon objects that are marked as
     *                     required
     * @param _properties  The sdfProperty quality: a vector of pointers to
     *                     subordinate sdfProperty objects
     * @param _actions     The sdfAction quality: a vector of pointers to
     *                     subordinate sdfAction objects
     * @param _events      The sdfEvent quality: a vector of pointers to
     *                     subordinate sdfEvent objects
     * @param _datatypes   The sdfData quality: a vector of pointers to
     *                     subordinate sdfData objects
     * @param _parentThing The superordinate sdfThing
     */
    sdfObject(std::string _name = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            std::vector<sdfProperty*> _properties = {},
            std::vector<sdfAction*> _actions = {},
            std::vector<sdfEvent*> _events = {},
            std::vector<sdfData*> _datatypes = {},
            sdfThing *_parentThing = NULL);

    /**
     * The sdfObject destructor.
     *
     * A destructor is needed to free the various pointers used in the member
     * variables.
     */
    virtual ~sdfObject();

    // setters
    /**
     * Setter function for the info member variable
     *
     * @param _info The new value of the info member variable.
     *
     * @sa info
     */
    void setInfo(sdfInfoBlock *_info);

    /**
     * Setter function for the ns member variable
     *
     * @param _ns The new value of the ns member variable.
     *
     * @sa ns
     */
    void setNamespace(sdfNamespaceSection *_ns);

    /**
     * Add an sdfProperty object pointer to the for the properties member variable
     *
     * @param property The new value to add to the properties member variable.
     *
     * @sa properties
     */
    void addProperty(sdfProperty *property);

    /**
     * Add an sdfAction object pointer to the for the actions member variable
     *
     * @param action The new value to add to the actions member variable.
     *
     * @sa actions
     */
    void addAction(sdfAction *action);

    /**
     * Add an sdfEvent object pointer to the for the events member variable
     *
     * @param event The new value to add to the events member variable.
     *
     * @sa events
     */
    void addEvent(sdfEvent *event);

    /**
     * Add an sdfData object pointer to the for the datatypes member variable
     *
     * @param datatype The new value to add to the datatypes member variable.
     *
     * @sa datatypes
     */
    void addDatatype(sdfData *datatype);

    /**
     * Setter function for the parent member variable
     *
     * @param parentThing The new value of the parent member variable.
     *
     * @sa parent
     */
    void setParentThing(sdfThing *parentThing);

    // getters
    /**
     * Getter function for the member variable info
     *
     * @return The value of the member variable info.
     *
     * @sa info
     */
    sdfInfoBlock* getInfo();

    /**
     * Getter function for the member variable ns
     *
     * @return The value of the member variable ns.
     *
     * @sa ns
     */
    sdfNamespaceSection* getNamespace();

    /**
     * Getter function for the member variable properties
     *
     * @return The value of the member variable properties.
     *
     * @sa properties
     */
    std::vector<sdfProperty*> getProperties();

    /**
     * Getter function for the member variable actions
     *
     * @return The value of the member variable actions.
     *
     * @sa actions
     */
    std::vector<sdfAction*> getActions();

    /**
     * Getter function for the member variable events
     *
     * @return The value of the member variable events.
     *
     * @sa events
     */
    std::vector<sdfEvent*> getEvents();

    /**
     * Getter function for the member variable datatypes.
     *
     * @return The value of the member variable datatypes.
     *
     * @sa datatypes
     */
    std::vector<sdfData*> getDatatypes();

    /**
     * Getter function for the member variable parent.
     *
     * @return The value of the member variable parent.
     *
     * @sa parent
     */
    sdfThing* getParentThing() const;

    /**
     * Getter function for the member variable parent cast to a sdfCommmon
     * pointer.
     *
     * @return The value of the member variable parent as a sdfCommon pointer.
     *
     * @sa parent
     */
    sdfCommon* getParent() const;

    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);

    /**
     * Transfer the information from this sdfObject object into a
     * JSON object.
     * The sdfInfoBlock and sdfNamespaceSection should be specified
     * in the superordinate sdfFile object. This used to be different so a
     * parameter was needed to specifiy whether or not this is a top-level
     * sdfObject and an sdfInfoBlock and sdfNamespaceSection need to be printed.
     *
     * @param prefix               JSON object to add to (prefix does not mean
     *                             the same prefix as in the namespace prefix)
     * @param print_info_namespace Whether or not to print an sdfInfoBlock and
     *                             sdfNamespaceSection
     *
     * @return The completed JSON object
     */
    nlohmann::json objectToJson(nlohmann::json prefix,
            bool print_info_namespace = true);

    /**
     * Print the information from this sdfObject object into a JSON object first
     * and then into a string. This function uses objectToJson().
     * The sdfInfoBlock and sdfNamespaceSection should be specified
     * in the superordinate sdfFile object. This used to be different so a
     * parameter was needed to specifiy whether or not this is a top-level
     * sdfObject and an sdfInfoBlock and sdfNamespaceSection need to be printed.
     *
     * @param print_info_namespace Whether or not to print an sdfInfoBlock and
     *                             sdfNamespaceSection
     *
     * @return The completed string
     */
    std::string objectToString(bool print_info_namespace = true);

    /**
     * Print the information from this sdfObject object into a JSON object
     * first, then into a string and then into a file.
     * This function uses objectToJson() and objectToString().
     *
     * @param path The path to the output file
     */
    void objectToFile(std::string path);

    /**
     * Transfer the information from a given JSON object into this
     * sdfObject object. In the early stage of the development of the
     * SDF/YANG converter there was no sdfFile class. Both jsonToObject() and
     * jsonToThing() were called to test whether the top-level structure was an
     * sdfObject or an sdfThing. This is what the testForThing parameter is for.
     *
     * @param input        The input JSON object
     * @param testForThing If true, look if input is has an sdfThing on the
     *                     top-level
     */
    sdfObject* jsonToObject(nlohmann::json input, bool testForThing = false);

    /**
     * Transfer the information from a file given by its name into this
     * sdfObject object. This function uses jsonToObject().
     * In the early stage of the development of the
     * SDF/YANG converter there was no sdfFile class. Both fileToObject() and
     * fileToThing() were called to test whether the top-level structure was an
     * sdfObject or an sdfThing. This is what the testForThing parameter is for.
     *
     * @param path        The path to the input file
     * @param testForThing If true, look if input is has an sdfThing on the
     *                     top-level
     */
    sdfObject* fileToObject(std::string path, bool testForThing = false);

private:
    /**
     * Represents the information block of an SDF model with nothing but
     * this sdfObject. This member variable is mostly not needed anymore
     * because the information block should be specified in the superordinate
     * sdfFile object.
     * @sa setInfo() and getInfo()
     */
    sdfInfoBlock *info;

    /**
     * Represents the namespace section of an SDF model with nothing but
     * this sdfObject. This member variable is mostly not needed anymore
     * because the namespace section should be specified in the superordinate
     * sdfFile object.
     * @sa setNamespace() and getNamespace()
     */
    sdfNamespaceSection *ns;

    /**
     * Represents the sdfProperty quality and contains a vector of subordinate
     * sdfProperty objects.
     * @sa addProperty() and getProperties()
     */
    std::vector<sdfProperty*> properties;

    /**
     * Represents the sdfAction quality and contains a vector of subordinate
     * sdfAction objects.
     * @sa addAction() and getActions()
     */
    std::vector<sdfAction*> actions;

    /**
     * Represents the sdfEvent quality and contains a vector of subordinate
     * sdfEvent objects.
     * @sa addEvent() and getEvents()
     */
    std::vector<sdfEvent*> events;

    /**
     * Represents the sdfData quality and contains a vector of subordinate
     * sdfData objects that serve as data type definitions.
     * @sa addDatatype() and getDatatypes()
     */
    std::vector<sdfData*> datatypes;

    /**
     * This member variable holds the superordinate sdfThing this sdfObject
     * can optionally belong to.
     * @sa getParentThing() and getParent()
     */
    sdfThing *parent;
};

/**
 * This class inherits from the sdfCommon class and represents sdfThings.
 */
class sdfThing : virtual public sdfCommon
{
public:
    /**
     * The sdfThing constructor
     *
     * @param _name        The name quality
     * @param _description The description quality
     * @param _reference   The sdfRef quality: a pointer to another
     *                     referenced sdfCommon object
     * @param _required    The sdfRequired quality: a vector of pointers to
     *                     subordinate sdfCommon objects that are marked as
     *                     required
     * @param _things      The sdfThing quality: a vector of pointers to
     *                     subordinate sdfThing objects
     * @param _objects     The sdfObject quality: a vector of pointers to
     *                     subordinate sdfObject objects
     * @param _parentThing The superordinate sdfThing
     */
    sdfThing(std::string _name = "",
            std::string _description = "",
            sdfCommon *_reference = NULL,
            std::vector<sdfCommon*> _required = {},
            std::vector<sdfThing*> _things = {},
            std::vector<sdfObject*> _objects = {},
            sdfThing *_parentThing = NULL);

    /**
     * The sdfThing destructor.
     *
     * A destructor is needed to free the various pointers used in the member
     * variables.
     */
    virtual ~sdfThing();
    // setters
    /**
     * Setter function for the info member variable
     *
     * @param _info The new value of the info member variable.
     *
     * @sa info
     */
    void setInfo(sdfInfoBlock *_info);

    /**
     * Setter function for the ns member variable
     *
     * @param _ns The new value of the ns member variable.
     *
     * @sa ns
     */
    void setNamespace(sdfNamespaceSection *_ns);

    /**
     * Add an sdfThing object pointer to the for the childThings member variable
     *
     * @param thing The new value to add to the childThings member variable.
     *
     * @sa childThings
     */
    void addThing(sdfThing *thing);

    /**
     * Add an sdfObject object pointer to the for the childObjects member
     * variable.
     *
     * @param object The new value to add to the childObjects member variable.
     *
     * @sa childObjects
     */
    void addObject(sdfObject *object);

    /**
     * Setter function for the parent member variable
     *
     * @param parentThing The new value of the parent member variable.
     *
     * @sa parent
     */
    void setParentThing(sdfThing *parentThing);
    // getters
    /**
     * Getter function for the member variable info
     *
     * @return The value of the member variable info.
     *
     * @sa info
     */
    sdfInfoBlock* getInfo() const;

    /**
     * Getter function for the member variable ns
     *
     * @return The value of the member variable ns.
     *
     * @sa ns
     */
    sdfNamespaceSection* getNamespace() const;

    /**
     * Getter function for the member variable childThings
     *
     * @return The value of the member variable childThings.
     *
     * @sa childThings
     */
    std::vector<sdfThing*> getThings() const;

    /**
     * Getter function for the member variable childObjects
     *
     * @return The value of the member variable childObjects.
     *
     * @sa childObjects
     */
    std::vector<sdfObject*> getObjects() const;

    /**
     * Getter function for the member variable parent
     *
     * @return The value of the member variable parent.
     *
     * @sa parent
     */
    sdfThing* getParentThing() const;

    /**
     * Getter function for the member variable parent cast to a sdfCommmon
     * pointer.
     *
     * @return The value of the member variable parent as a sdfCommon pointer.
     *
     * @sa parent
     */
    sdfCommon* getParent() const;

    // parsing
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);

    /**
     * Transfer the information from this sdfThing object into a
     * JSON object. The sdfInfoBlock and sdfNamespaceSection should be specified
     * in the superordinate sdfFile object. This used to be different so a
     * parameter was needed to specifiy whether or not this is a top-level
     * sdfThing and an sdfInfoBlock and sdfNamespaceSection need to be printed.
     *
     * @param prefix               JSON object to add to (prefix does not mean
     *                             the same prefix as in the namespace prefix)
     * @param print_info_namespace Whether or not to print an sdfInfoBlock and
     *                             sdfNamespaceSection
     *
     * @return The completed JSON object
     */
    nlohmann::json thingToJson(nlohmann::json prefix,
            bool print_info_namespace = false);

    /**
     * Print the information from this sdfThing object into a JSON object first
     * and then into a string. This function uses thingToJson().
     * The sdfInfoBlock and sdfNamespaceSection should be specified
     * in the superordinate sdfFile object. This used to be different so a
     * parameter was needed to specifiy whether or not this is a top-level
     * sdfThing and an sdfInfoBlock and sdfNamespaceSection need to be printed.
     *
     * @param print_info_namespace Whether or not to print an sdfInfoBlock and
     *                             sdfNamespaceSection
     *
     * @return The completed string
     */
    std::string thingToString(bool print_info_namespace = true);

    /**
     * Print the information from this sdfThing object into a JSON object first,
     * then into a string and then into a file.
     * This function uses thingToJson() and thingToString().
     *
     * @param path The path to the output file
     */
    void thingToFile(std::string path);

    /**
     * Transfer the information from a given JSON object into this
     * sdfThing object.
     *
     * @param input  The input JSON object
     * @param nested If true, this sdfThing belongs to another sdfThing and
     *               needs to be handled differently as a top-level sdfThing
     */
    sdfThing* jsonToThing(nlohmann::json input, bool nested = false);

    /**
     * Transfer the information from a given JSON object into this (nested)
     * sdfThing object.
     *
     * @param input  The input JSON object
     */
    sdfThing* jsonToNestedThing(nlohmann::json input);

    /**
     * Transfer the information from a file given by its path into this
     * sdfThing object. This function uses jsonToThing().
     *
     * @param path  The path to the input file
     */
    sdfThing* fileToThing(std::string path);

private:
    /**
     * Represents the information block of an SDF model with nothing but
     * this sdfThing. This member variable is mostly not needed anymore
     * because the information block should be specified in the superordinate
     * sdfFile object.
     * @sa setInfo() and getInfo()
     */
    sdfInfoBlock *info;

    /**
     * Represents the namespace section of an SDF model with nothing but
     * this sdfThing. This member variable is mostly not needed anymore
     * because the namespace section should be specified in the superordinate
     * sdfFile object.
     * @sa setNamespace() and getNamespace()
     */
    sdfNamespaceSection *ns;

    /**
     * Represents the sdfThing quality and contains a vector of subordinate
     * sdfThing objects.
     * @sa addThing() and getThings()
     */
    std::vector<sdfThing*> childThings;

    /**
     * Represents the sdfObject quality and contains a vector of subordinate
     * sdfObject objects.
     * @sa addObject() and getObjects()
     */
    std::vector<sdfObject*> childObjects;

    /**
     * This member variable holds the superordinate sdfThing this sdfThing
     * can optionally belong to.
     * @sa getParentThing() and getParent()
     */
    sdfThing *parent;

};

/*
 * TODO?
 * Products may be composed of Objects and Things at the high level,
 * and may include their own definitions of Properties, Actions, and
 * Events that can be used to extend or complete the included Object
 * definitions. Product definitions may set optional defaults and
 * constant values for specific use cases, for example units, range,
 * and scale settings for properties, or available parameters for Actions
 */
/*
class sdfProduct : sdfThing
{};
*/

/**
 * This class can be used to represent SDF models as a whole.
 */
class sdfFile
{
public:
    /**
     * The sdfFile constructor. Initialises all member variables.
     */
    sdfFile();

    /**
     * The sdfFile destructor.
     *
     * A destructor is needed to free the various pointers used in the member
     * variables.
     */
    ~sdfFile();
    // setters
    /**
     * Setter function for the info member variable
     *
     * @param _info The new value of the info member variable.
     *
     * @sa info
     */
    void setInfo(sdfInfoBlock *_info);

    /**
     * Setter function for the ns member variable
     *
     * @param _ns The new value of the ns member variable.
     *
     * @sa ns
     */
    void setNamespace(sdfNamespaceSection *_ns);

    /**
     * Add an sdfThing object pointer to the for the things member variable
     *
     * @param thing The new value to add to the things member variable.
     *
     * @sa things
     */
    void addThing(sdfThing *thing);

    /**
     * Add an sdfObject object pointer to the for the objects member variable
     *
     * @param object The new value to add to the objects member variable.
     *
     * @sa objects
     */
    void addObject(sdfObject *object);

    /**
     * Add an sdfProperty object pointer to the for the properties member variable
     *
     * @param property The new value to add to the properties member variable.
     *
     * @sa properties
     */
    void addProperty(sdfProperty *property);

    /**
     * Add an sdfAction object pointer to the for the actions member variable
     *
     * @param action The new value to add to the actions member variable.
     *
     * @sa actions
     */
    void addAction(sdfAction *action);

    /**
     * Add an sdfEvent object pointer to the for the events member variable
     *
     * @param event The new value to add to the events member variable.
     *
     * @sa events
     */
    void addEvent(sdfEvent *event);

    /**
     * Add an sdfData object pointer to the for the datatypes member variable
     *
     * @param datatype The new value to add to the datatypes member variable.
     *
     * @sa datatypes
     */
    void addDatatype(sdfData *datatype);
    // getters
    /**
     * Getter function for the member variable info
     *
     * @return The value of the member variable info.
     *
     * @sa info
     */
    sdfInfoBlock* getInfo() const;

    /**
     * Getter function for the member variable ns
     *
     * @return The value of the member variable ns.
     *
     * @sa ns
     */
    sdfNamespaceSection* getNamespace() const;

    /**
     * Getter function for the member variable things
     *
     * @return The value of the member variable things.
     *
     * @sa things
     */
    std::vector<sdfThing*> getThings() const;

    /**
     * Getter function for the member variable objects
     *
     * @return The value of the member variable objects.
     *
     * @sa objects
     */
    std::vector<sdfObject*> getObjects() const;

    /**
     * Getter function for the member variable properties
     *
     * @return The value of the member variable properties.
     *
     * @sa properties
     */
    std::vector<sdfProperty*> getProperties();

    /**
     * Getter function for the member variable actions
     *
     * @return The value of the member variable actions.
     *
     * @sa actions
     */
    std::vector<sdfAction*> getActions();

    /**
     * Getter function for the member variable events.
     *
     * @return The value of the member variable events.
     *
     * @sa events
     */
    std::vector<sdfEvent*> getEvents();

    /**
     * Getter function for the member variable datatypes.
     *
     * @return The value of the member variable datatypes.
     *
     * @sa datatypes
     */
    std::vector<sdfData*> getDatatypes();

    // parsing
    /**
     * Generate a string that contains a reference to a
     * given subordinate sdfCommon as required by the sdfRef quality.
     *
     * This function cannot be called on child directly because the class of
     * the superordinate element of child must be known since it is stated in
     * the reference string (e.g. "#/sdfObject/sensor/sdfData/temperatureData").
     * Each class has its own version of this function to be able to append the
     * appropriate class name.
     *
     * @param child  A pointer to the subordinate sdfCommon that the reference
     *               string is to be created for
     * @param import Set to true if this sdfCommon is referenced from another
     *               model and the reference string needs a prefix
     *
     * @return The reference string to refer to child
     */
    std::string generateReferenceString(sdfCommon *child = NULL,
            bool import = false);

    /**
     * Transfer the information from this sdfFile object into a
     * JSON object
     *
     * @param prefix JSON object to add to (prefix does not mean the same prefix
     *               as in the namespace prefix)
     *
     * @return The completed JSON object
     */
    nlohmann::json toJson(nlohmann::json prefix);

    /**
     * Print the information from this sdfFile object into a JSON object first
     * and then into a string. This function uses toJson().
     *
     * @return The completed string
     */
    std::string toString();

    /**
     * Print the information from this sdfFile object into a JSON object
     * first, then into a string and then into a file.
     * This function uses toJson() and toString().
     *
     * @param path The path to the output file
     */
    void toFile(std::string path);

    /**
     * Transfer the information from a given JSON object into this
     * sdfFile object.
     *
     * @param input The input JSON object
     */
    sdfFile* fromJson(nlohmann::json input);

    /**
     * Transfer the information from a file given by its path into this
     * sdfFile object. This function uses fromJson().
     *
     * @param path  The path to the input file
     */
    sdfFile* fromFile(std::string path);
private:
    /**
     * Represents the information block of an SDF model.
     * @sa setInfo() and getInfo()
     */
    sdfInfoBlock *info;

    /**
     * Represents the namespace section of an SDF model.
     * @sa setNamespace() and getNamespace()
     */
    sdfNamespaceSection *ns;

    /**
     * Represents the sdfThing quality and contains a vector of the top-level
     * sdfThing objects.
     * @sa addThing() and getThings()
     */
    std::vector<sdfThing*> things;

    /**
     * Represents the sdfObject quality and contains a vector of the top-level
     * sdfObject objects.
     * @sa addObject() and getObjects()
     */
    std::vector<sdfObject*> objects;

    /**
     * Represents the sdfProperty quality and contains a vector of the top-level
     * sdfProperty objects.
     * @sa addProperty() and getProperties()
     */
    std::vector<sdfProperty*> properties;

    /**
     * Represents the sdfAction quality and contains a vector of the top-level
     * sdfAction objects.
     * @sa addAction() and getActions()
     */
    std::vector<sdfAction*> actions;

    /**
     * Represents the sdfEvent quality and contains a vector of the top-level
     * sdfEvent objects.
     * @sa addEvent() and getEvents()
     */
    std::vector<sdfEvent*> events;

    /**
     * Represents the sdfData quality and contains a vector of the top-level
     * sdfData objects that serve as data type definitions in the model.
     * @sa addDatatype() and getDatatypes()
     */
    std::vector<sdfData*> datatypes;
};

#endif

