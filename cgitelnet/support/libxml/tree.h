/*
 * tree.h : describes the structures found in an tree resulting
 *          from an XML parsing.
 *
 * See Copyright for the status of this software.
 *
 * Daniel.Veillard@w3.org
 */

#ifndef __XML_TREE_H__
#define __XML_TREE_H__

#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

/*
 * The different element types carried by an XML tree
 *
 * NOTE: This is synchronized with DOM Level1 values
 *       See http://www.w3.org/TR/REC-DOM-Level-1/
 */
typedef enum {
    XML_ELEMENT_NODE=		1,
    XML_ATTRIBUTE_NODE=		2,
    XML_TEXT_NODE=		3,
    XML_CDATA_SECTION_NODE=	4,
    XML_ENTITY_REF_NODE=	5,
    XML_ENTITY_NODE=		6,
    XML_PI_NODE=		7,
    XML_COMMENT_NODE=		8,
    XML_DOCUMENT_NODE=		9,
    XML_DOCUMENT_TYPE_NODE=	10,
    XML_DOCUMENT_FRAG_NODE=	11,
    XML_NOTATION_NODE=		12,
    XML_HTML_DOCUMENT_NODE=	13
} xmlElementType;

/*
 * Size of an internal character representation.
 *
 * Currently we use 8bit chars internal representation for memory efficiency,
 * but the parser is not tied to that, just define UNICODE to switch to
 * a 16 bits internal representation. Note that with 8 bits wide
 * xmlChars one can still use UTF-8 to handle correctly non ISO-Latin
 * input.
 */

#ifdef UNICODE
typedef unsigned short xmlChar;
#else
typedef unsigned char xmlChar;
#endif

#ifndef WIN32
#ifndef CHAR
#define CHAR xmlChar
#endif
#endif

#define BAD_CAST (xmlChar *)

/*
 * a DTD Notation definition
 */

typedef struct _xmlNotation xmlNotation;
typedef xmlNotation *xmlNotationPtr;
struct _xmlNotation {
    const xmlChar               *name;	/* Notation name */
    const xmlChar               *PublicID;	/* Public identifier, if any */
    const xmlChar               *SystemID;	/* System identifier, if any */
};

/*
 * a DTD Attribute definition
 */

typedef enum {
    XML_ATTRIBUTE_CDATA = 1,
    XML_ATTRIBUTE_ID,
    XML_ATTRIBUTE_IDREF	,
    XML_ATTRIBUTE_IDREFS,
    XML_ATTRIBUTE_ENTITY,
    XML_ATTRIBUTE_ENTITIES,
    XML_ATTRIBUTE_NMTOKEN,
    XML_ATTRIBUTE_NMTOKENS,
    XML_ATTRIBUTE_ENUMERATION,
    XML_ATTRIBUTE_NOTATION
} xmlAttributeType;

typedef enum {
    XML_ATTRIBUTE_NONE = 1,
    XML_ATTRIBUTE_REQUIRED,
    XML_ATTRIBUTE_IMPLIED,
    XML_ATTRIBUTE_FIXED
} xmlAttributeDefault;

typedef struct _xmlEnumeration xmlEnumeration;
typedef xmlEnumeration *xmlEnumerationPtr;
struct _xmlEnumeration {
    struct _xmlEnumeration    *next;	/* next one */
    const xmlChar            *name;	/* Enumeration name */
};

typedef struct _xmlAttribute xmlAttribute;
typedef xmlAttribute *xmlAttributePtr;
struct _xmlAttribute {
    const xmlChar         *elem;	/* Element holding the attribute */
    const xmlChar         *name;	/* Attribute name */
    struct _xmlAttribute   *next;       /* list of attributes of an element */
    xmlAttributeType       type;	/* The type */
    xmlAttributeDefault    def;		/* the default */
    const xmlChar         *defaultValue;/* or the default value */
    xmlEnumerationPtr      tree;        /* or the enumeration tree if any */
    const xmlChar         *prefix;      /* the namespace prefix if any */
};

/*
 * a DTD Element definition.
 */
typedef enum {
    XML_ELEMENT_CONTENT_PCDATA = 1,
    XML_ELEMENT_CONTENT_ELEMENT,
    XML_ELEMENT_CONTENT_SEQ,
    XML_ELEMENT_CONTENT_OR
} xmlElementContentType;

typedef enum {
    XML_ELEMENT_CONTENT_ONCE = 1,
    XML_ELEMENT_CONTENT_OPT,
    XML_ELEMENT_CONTENT_MULT,
    XML_ELEMENT_CONTENT_PLUS
} xmlElementContentOccur;

typedef struct _xmlElementContent xmlElementContent;
typedef xmlElementContent *xmlElementContentPtr;
struct _xmlElementContent {
    xmlElementContentType     type;	/* PCDATA, ELEMENT, SEQ or OR */
    xmlElementContentOccur    ocur;	/* ONCE, OPT, MULT or PLUS */
    const xmlChar            *name;	/* Element name */
    struct _xmlElementContent *c1;	/* first child */
    struct _xmlElementContent *c2;	/* second child */
};

typedef enum {
    XML_ELEMENT_TYPE_EMPTY = 1,
    XML_ELEMENT_TYPE_ANY,
    XML_ELEMENT_TYPE_MIXED,
    XML_ELEMENT_TYPE_ELEMENT
} xmlElementTypeVal;

typedef struct _xmlElement xmlElement;
typedef xmlElement *xmlElementPtr;
struct _xmlElement {
    const xmlChar          *name;	/* Element name */
    xmlElementTypeVal       type;	/* The type */
    xmlElementContentPtr content;	/* the allowed element content */
    xmlAttributePtr   attributes;	/* List of the declared attributes */
};

/*
 * An XML namespace.
 * Note that prefix == NULL is valid, it defines the default namespace
 * within the subtree (until overriden).
 */

typedef enum {
    XML_GLOBAL_NAMESPACE = 1,	/* old style global namespace */
    XML_LOCAL_NAMESPACE		/* new style local scoping */
} xmlNsType;

typedef struct _xmlNs xmlNs;
typedef xmlNs *xmlNsPtr;
struct _xmlNs {
    struct _xmlNs  *next;	/* next Ns link for this node  */
    xmlNsType      type;	/* global or local */
    const xmlChar *href;	/* URL for the namespace */
    const xmlChar *prefix;	/* prefix for the namespace */
};

/*
 * An XML DtD, as defined by <!DOCTYPE.
 */
typedef struct _xmlDtd xmlDtd;
typedef xmlDtd *xmlDtdPtr;
struct _xmlDtd {
    const xmlChar *name;	/* Name of the DTD */
    const xmlChar *ExternalID;	/* External identifier for PUBLIC DTD */
    const xmlChar *SystemID;	/* URI for a SYSTEM or PUBLIC DTD */
    void          *notations;   /* Hash table for notations if any */
    void          *elements;    /* Hash table for elements if any */
    void          *attributes;  /* Hash table for attributes if any */
    void          *entities;    /* Hash table for entities if any */
    /* struct xmlDtd *next;	 * next  link for this document  */
};

/*
 * A attribute of an XML node.
 */
typedef struct _xmlAttr xmlAttr;
typedef xmlAttr *xmlAttrPtr;
struct _xmlAttr {
#ifndef XML_WITHOUT_CORBA
    void           *_private;	/* for Corba, must be first ! */
    void           *vepv;	/* for Corba, must be next ! */
#endif
    xmlElementType  type;       /* XML_ATTRIBUTE_NODE, must be third ! */
    struct _xmlNode *node;	/* attr->node link */
    struct _xmlAttr *next;	/* attribute list link */
    const xmlChar   *name;      /* the name of the property */
    struct _xmlNode *val;       /* the value of the property */
    xmlNs           *ns;        /* pointer to the associated namespace */
};

/*
 * An XML ID instance.
 */

typedef struct _xmlID xmlID;
typedef xmlID *xmlIDPtr;
struct _xmlID {
    struct _xmlID    *next;	/* next ID */
    const xmlChar    *value;	/* The ID name */
    xmlAttrPtr        attr;	/* The attribut holding it */
};

/*
 * An XML IDREF instance.
 */

typedef struct _xmlRef xmlRef;
typedef xmlRef *xmlRefPtr;
struct _xmlRef {
    struct _xmlRef    *next;	/* next Ref */
    const xmlChar     *value;	/* The Ref name */
    xmlAttrPtr        attr;	/* The attribut holding it */
};

/*
 * A buffer structure
 */

typedef enum {
    XML_BUFFER_ALLOC_DOUBLEIT,
    XML_BUFFER_ALLOC_EXACT
} xmlBufferAllocationScheme;

typedef struct _xmlBuffer xmlBuffer;
typedef xmlBuffer *xmlBufferPtr;
struct _xmlBuffer {
    xmlChar *content;		/* The buffer content UTF8 */
    unsigned int use;		/* The buffer size used */
    unsigned int size;		/* The buffer size */
    xmlBufferAllocationScheme alloc; /* The realloc method */
};

/*
 * A node in an XML tree.
 */
typedef struct _xmlNode xmlNode;
typedef xmlNode *xmlNodePtr;
struct _xmlNode {
#ifndef XML_WITHOUT_CORBA
    void           *_private;	/* for Corba, must be first ! */
    void           *vepv;	/* for Corba, must be next ! */
#endif
    xmlElementType  type;	/* type number in the DTD, must be third ! */
    struct _xmlDoc  *doc;	/* the containing document */
    struct _xmlNode *parent;	/* child->parent link */
    struct _xmlNode *next;	/* next sibling link  */
    struct _xmlNode *prev;	/* previous sibling link  */
    struct _xmlNode *childs;	/* parent->childs link */
    struct _xmlNode *last;	/* last child link */
    struct _xmlAttr *properties;/* properties list */
    const xmlChar  *name;       /* the name of the node, or the entity */
    xmlNs          *ns;         /* pointer to the associated namespace */
    xmlNs          *nsDef;      /* namespace definitions on this node */
#ifndef XML_USE_BUFFER_CONTENT    
    xmlChar        *content;    /* the content */
#else
    xmlBufferPtr   content;     /* the content in a buffer */
#endif
};

/*
 * An XML document.
 */
typedef struct _xmlDoc xmlDoc;
typedef xmlDoc *xmlDocPtr;
struct _xmlDoc {
#ifndef XML_WITHOUT_CORBA
    void           *_private;	/* for Corba, must be first ! */
    void           *vepv;	/* for Corba, must be next ! */
#endif
    xmlElementType  type;       /* XML_DOCUMENT_NODE, must be second ! */
    char           *name;	/* name/filename/URI of the document */
    const xmlChar  *version;	/* the XML version string */
    const xmlChar  *encoding;   /* encoding, if any */
    int             compression;/* level of zlib compression */
    int             standalone; /* standalone document (no external refs) */
    struct _xmlDtd  *intSubset;	/* the document internal subset */
    struct _xmlDtd  *extSubset;	/* the document external subset */
    struct _xmlNs   *oldNs;	/* Global namespace, the old way */
    struct _xmlNode *root;	/* the document tree */
    void           *ids;        /* Hash table for ID attributes if any */
    void           *refs;       /* Hash table for IDREFs attributes if any */
};

/*
 * Variables.
 */
extern xmlNsPtr baseDTD;
extern int oldXMLWDcompatibility;/* maintain compatibility with old WD */
extern int xmlIndentTreeOutput;  /* try to indent the tree dumps */
extern xmlBufferAllocationScheme xmlBufferAllocScheme; /* alloc scheme to use */
extern int xmlSaveNoEmptyTags;   /* save empty tags as <empty></empty> */

/*
 * Handling Buffers.
 */

xmlBufferPtr	xmlBufferCreate		(void);
xmlBufferPtr	xmlBufferCreateSize	(size_t size);
void		xmlBufferFree		(xmlBufferPtr buf);
int		xmlBufferDump		(FILE *file,
					 xmlBufferPtr buf);
void		xmlBufferAdd		(xmlBufferPtr buf,
					 const xmlChar *str,
					 int len);
void		xmlBufferCat		(xmlBufferPtr buf,
					 const xmlChar *str);
void		xmlBufferCCat		(xmlBufferPtr buf,
					 const char *str);
int		xmlBufferShrink		(xmlBufferPtr buf,
					 int len);
void		xmlBufferEmpty		(xmlBufferPtr buf);
const xmlChar*	xmlBufferContent	(const xmlBufferPtr buf);
int		xmlBufferUse		(const xmlBufferPtr buf);
void		xmlBufferSetAllocationScheme(xmlBufferPtr buf,
					 xmlBufferAllocationScheme scheme);
int		xmlBufferLength		(const xmlBufferPtr buf);

/*
 * Creating/freeing new structures
 */
xmlDtdPtr	xmlCreateIntSubset	(xmlDocPtr doc,
					 const xmlChar *name,
					 const xmlChar *ExternalID,
					 const xmlChar *SystemID);
xmlDtdPtr	xmlNewDtd		(xmlDocPtr doc,
					 const xmlChar *name,
					 const xmlChar *ExternalID,
					 const xmlChar *SystemID);
void		xmlFreeDtd		(xmlDtdPtr cur);
xmlNsPtr	xmlNewGlobalNs		(xmlDocPtr doc,
					 const xmlChar *href,
					 const xmlChar *prefix);
xmlNsPtr	xmlNewNs		(xmlNodePtr node,
					 const xmlChar *href,
					 const xmlChar *prefix);
void		xmlFreeNs		(xmlNsPtr cur);
xmlDocPtr 	xmlNewDoc		(const xmlChar *version);
void		xmlFreeDoc		(xmlDocPtr cur);
xmlAttrPtr	xmlNewDocProp		(xmlDocPtr doc,
					 const xmlChar *name,
					 const xmlChar *value);
xmlAttrPtr	xmlNewProp		(xmlNodePtr node,
					 const xmlChar *name,
					 const xmlChar *value);
xmlAttrPtr	xmlNewNsProp		(xmlNodePtr node,
					 xmlNsPtr ns,
					 const xmlChar *name,
					 const xmlChar *value);
void		xmlFreePropList		(xmlAttrPtr cur);
void		xmlFreeProp		(xmlAttrPtr cur);
xmlAttrPtr	xmlCopyProp		(xmlNodePtr target,
					 xmlAttrPtr cur);
xmlAttrPtr	xmlCopyPropList		(xmlNodePtr target,
					 xmlAttrPtr cur);
xmlDtdPtr	xmlCopyDtd		(xmlDtdPtr dtd);
xmlDocPtr	xmlCopyDoc		(xmlDocPtr doc,
					 int recursive);

/*
 * Creating new nodes
 */
xmlNodePtr	xmlNewDocNode		(xmlDocPtr doc,
					 xmlNsPtr ns,
					 const xmlChar *name,
					 const xmlChar *content);
xmlNodePtr	xmlNewDocRawNode	(xmlDocPtr doc,
					 xmlNsPtr ns,
					 const xmlChar *name,
					 const xmlChar *content);
xmlNodePtr	xmlNewNode		(xmlNsPtr ns,
					 const xmlChar *name);
xmlNodePtr	xmlNewChild		(xmlNodePtr parent,
					 xmlNsPtr ns,
					 const xmlChar *name,
					 const xmlChar *content);
xmlNodePtr	xmlNewTextChild		(xmlNodePtr parent,
					 xmlNsPtr ns,
					 const xmlChar *name,
					 const xmlChar *content);
xmlNodePtr	xmlNewDocText		(xmlDocPtr doc,
					 const xmlChar *content);
xmlNodePtr	xmlNewText		(const xmlChar *content);
xmlNodePtr	xmlNewPI		(const xmlChar *name,
					 const xmlChar *content);
xmlNodePtr	xmlNewDocTextLen	(xmlDocPtr doc,
					 const xmlChar *content,
					 int len);
xmlNodePtr	xmlNewTextLen		(const xmlChar *content,
					 int len);
xmlNodePtr	xmlNewDocComment	(xmlDocPtr doc,
					 const xmlChar *content);
xmlNodePtr	xmlNewComment		(const xmlChar *content);
xmlNodePtr	xmlNewCDataBlock	(xmlDocPtr doc,
					 const xmlChar *content,
					 int len);
xmlNodePtr	xmlNewReference		(xmlDocPtr doc,
					 const xmlChar *name);
xmlNodePtr	xmlCopyNode		(xmlNodePtr node,
					 int recursive);
xmlNodePtr	xmlCopyNodeList		(xmlNodePtr node);
xmlNodePtr	xmlNewDocFragment	(xmlDocPtr doc);

/*
 * Navigating
 */
xmlNodePtr	xmlDocGetRootElement	(xmlDocPtr doc);
xmlNodePtr	xmlGetLastChild		(xmlNodePtr parent);
int		xmlNodeIsText		(xmlNodePtr node);
int		xmlIsBlankNode		(xmlNodePtr node);

/*
 * Changing the structure
 */
xmlNodePtr	xmlDocSetRootElement	(xmlDocPtr doc,
					 xmlNodePtr root);
void		xmlNodeSetName		(xmlNodePtr cur,
					 const xmlChar *name);
xmlNodePtr	xmlAddChild		(xmlNodePtr parent,
					 xmlNodePtr cur);
xmlNodePtr	xmlReplaceNode		(xmlNodePtr old,
					 xmlNodePtr cur);
xmlNodePtr	xmlAddSibling		(xmlNodePtr cur,
					 xmlNodePtr elem);
xmlNodePtr	xmlAddPrevSibling	(xmlNodePtr cur,
					 xmlNodePtr elem);
xmlNodePtr	xmlAddNextSibling	(xmlNodePtr cur,
					 xmlNodePtr elem);
void		xmlUnlinkNode		(xmlNodePtr cur);
xmlNodePtr	xmlTextMerge		(xmlNodePtr first,
					 xmlNodePtr second);
void		xmlTextConcat		(xmlNodePtr node,
					 const xmlChar *content,
					 int len);
void		xmlFreeNodeList		(xmlNodePtr cur);
void		xmlFreeNode		(xmlNodePtr cur);
int		xmlRemoveProp		(xmlAttrPtr cur);

/*
 * Namespaces
 */
xmlNsPtr	xmlSearchNs		(xmlDocPtr doc,
					 xmlNodePtr node,
					 const xmlChar *nameSpace);
xmlNsPtr	xmlSearchNsByHref	(xmlDocPtr doc,
					 xmlNodePtr node,
					 const xmlChar *href);
xmlNsPtr *	xmlGetNsList		(xmlDocPtr doc,
					 xmlNodePtr node);
void		xmlSetNs		(xmlNodePtr node,
					 xmlNsPtr ns);
xmlNsPtr	xmlCopyNamespace	(xmlNsPtr cur);
xmlNsPtr	xmlCopyNamespaceList	(xmlNsPtr cur);

/*
 * Changing the content.
 */
xmlAttrPtr	xmlSetProp		(xmlNodePtr node,
					 const xmlChar *name,
					 const xmlChar *value);
xmlChar *	xmlGetProp		(xmlNodePtr node,
					 const xmlChar *name);
xmlChar *	xmlGetNsProp		(xmlNodePtr node,
					 const xmlChar *name,
					 const xmlChar *nameSpace);
xmlNodePtr	xmlStringGetNodeList	(xmlDocPtr doc,
					 const xmlChar *value);
xmlNodePtr	xmlStringLenGetNodeList	(xmlDocPtr doc,
					 const xmlChar *value,
					 int len);
xmlChar *	xmlNodeListGetString	(xmlDocPtr doc,
					 xmlNodePtr list,
					 int inLine);
void		xmlNodeSetContent	(xmlNodePtr cur,
					 const xmlChar *content);
void		xmlNodeSetContentLen	(xmlNodePtr cur,
					 const xmlChar *content,
					 int len);
void		xmlNodeAddContent	(xmlNodePtr cur,
					 const xmlChar *content);
void		xmlNodeAddContentLen	(xmlNodePtr cur,
					 const xmlChar *content,
					 int len);
xmlChar *	xmlNodeGetContent	(xmlNodePtr cur);
xmlChar *	xmlNodeGetLang		(xmlNodePtr cur);
void		xmlNodeSetLang		(xmlNodePtr cur,
					 const xmlChar *lang);
xmlChar *	xmlNodeGetBase		(xmlDocPtr doc,
					 xmlNodePtr cur);

/*
 * Removing content.
 */
int		xmlRemoveProp		(xmlAttrPtr attr); /* TODO */
int		xmlRemoveNode		(xmlNodePtr node); /* TODO */

/*
 * Internal, don't use
 */
void		xmlBufferWriteCHAR	(xmlBufferPtr buf,
					 const xmlChar *string);
void		xmlBufferWriteChar	(xmlBufferPtr buf,
					 const char *string);
void		xmlBufferWriteQuotedString(xmlBufferPtr buf,
					 const xmlChar *string);

/*
 * Saving
 */
void		xmlDocDumpMemory	(xmlDocPtr cur,
					 xmlChar**mem,
					 int *size);
void		xmlDocDump		(FILE *f,
					 xmlDocPtr cur);
void		xmlElemDump		(FILE *f,
					 xmlDocPtr cur,
					 xmlNodePtr elem);
int		xmlSaveFile		(const char *filename,
					 xmlDocPtr cur);

/*
 * Compression
 */
int		xmlGetDocCompressMode	(xmlDocPtr doc);
void		xmlSetDocCompressMode	(xmlDocPtr doc,
					 int mode);
int		xmlGetCompressMode	(void);
void		xmlSetCompressMode	(int mode);

#ifdef __cplusplus
}
#endif

#endif /* __XML_TREE_H__ */

