// class_system.h - Complete C class system with OOP features
#ifndef CLASS_SYSTEM_H
#define CLASS_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ====================== Core Runtime Macros ======================

// Type definitions
#define CLASS(name) typedef struct name name; struct name { \
    struct name##_vtable* vtable; 

#define EXTENDS(parent) parent parent_obj;

#define END_CLASS };

#define VTABLE(name) typedef struct name##_vtable name##_vtable; \
    struct name##_vtable {

#define END_VTABLE };

// Method declaration in vtable
#define METHOD(ret_type, name, ...) ret_type (*name)(void* self, ##__VA_ARGS__)

// Method implementation
#define DEFINE_METHOD(class_name, ret_type, name, ...) \
    ret_type class_name##_##name(class_name* self, ##__VA_ARGS__)

// Constructor
#define CONSTRUCTOR(class_name, ...) \
    class_name* class_name##_new(__VA_ARGS__)

// Instance creation
#define new(class_name, ...) class_name##_new(__VA_ARGS__)

// Destructor
#define DEFINE_DESTRUCTOR(class_name) \
    void class_name##_destroy(class_name* self)

// Delete object
#define delete(obj) do { \
    if (obj && obj->vtable && obj->vtable->destroy) { \
        obj->vtable->destroy(obj); \
    } else { \
        free(obj); \
    } \
    obj = NULL; \
} while(0)

// Property access
#define self(obj) obj->
#define super(obj) obj->parent_obj.

// Method call
#define call(obj, method, ...) obj->vtable->method(obj, ##__VA_ARGS__)

// ====================== Base Object Class ======================

// Base Object class definition
CLASS(Object)
    // Base class has no parent, just core data
    char* type_name;
END_CLASS

VTABLE(Object)
    METHOD(char*, toString);
    METHOD(void, destroy);
END_VTABLE

// Object methods implementation
DEFINE_METHOD(Object, char*, toString) {
    char* result = malloc(128);
    if (result) {
        sprintf(result, "Object of type %s", self->type_name);
    }
    return result;
}

DEFINE_DESTRUCTOR(Object) {
    free(self);
}

// Object constructor
CONSTRUCTOR(Object) {
    Object* self = malloc(sizeof(Object));
    if (!self) return NULL;
    
    static Object_vtable vtable = {
        .toString = (char* (*)(void*))Object_toString,
        .destroy = (void (*)(void*))Object_destroy
    };
    
    self->vtable = &vtable;
    self->type_name = "Object";
    
    return self;
}

// ====================== Person Example Class ======================

// Person class definition
CLASS(Person)
    EXTENDS(Object)
    char* name;
    int age;
END_CLASS

VTABLE(Person)
    METHOD(char*, toString);
    METHOD(void, destroy);
    METHOD(void, greet);
    METHOD(int, getAge);
END_VTABLE

// Person methods implementation
DEFINE_METHOD(Person, char*, toString) {
    char* result = malloc(128);
    if (result) {
        sprintf(result, "Person: name=%s, age=%d", self->name, self->age);
    }
    return result;
}

DEFINE_DESTRUCTOR(Person) {
    free(self->name);
    free(self);
}

DEFINE_METHOD(Person, void, greet) {
    printf("Hello, my name is %s!\n", self->name);
}

DEFINE_METHOD(Person, int, getAge) {
    return self->age;
}

// Person constructor
CONSTRUCTOR(Person, const char* name, int age) {
    Person* self = malloc(sizeof(Person));
    if (!self) return NULL;
    
    static Person_vtable vtable = {
        .toString = (char* (*)(void*))Person_toString,
        .destroy = (void (*)(void*))Person_destroy,
        .greet = (void (*)(void*))Person_greet,
        .getAge = (int (*)(void*))Person_getAge
    };
    
    self->vtable = &vtable;
    self->name = strdup(name);
    self->age = age;
    
    // Initialize parent part
    self->parent_obj.type_name = "Person";
    
    return self;
}

// ====================== Student Example Subclass ======================

// Student class definition (extends Person)
CLASS(Student)
    EXTENDS(Person)
    char* school;
    double gpa;
END_CLASS

VTABLE(Student)
    METHOD(char*, toString);
    METHOD(void, destroy);
    METHOD(void, greet);
    METHOD(int, getAge);
    METHOD(void, study);
    METHOD(double, getGPA);
END_VTABLE

// Student methods implementation
DEFINE_METHOD(Student, char*, toString) {
    char* result = malloc(256);
    if (result) {
        sprintf(result, "Student: name=%s, age=%d, school=%s, GPA=%.2f", 
                self->parent_obj.name, self->parent_obj.age, self->school, self->gpa);
    }
    return result;
}

DEFINE_DESTRUCTOR(Student) {
    free(self->school);
    free(self->parent_obj.name);
    free(self);
}

DEFINE_METHOD(Student, void, greet) {
    printf("Hello, I'm %s, a student at %s!\n", self->parent_obj.name, self->school);
}

DEFINE_METHOD(Student, int, getAge) {
    return self->parent_obj.age;
}

DEFINE_METHOD(Student, void, study) {
    printf("%s is studying hard!\n", self->parent_obj.name);
}

DEFINE_METHOD(Student, double, getGPA) {
    return self->gpa;
}

// Student constructor
CONSTRUCTOR(Student, const char* name, int age, const char* school, double gpa) {
    Student* self = malloc(sizeof(Student));
    if (!self) return NULL;
    
    static Student_vtable vtable = {
        .toString = (char* (*)(void*))Student_toString,
        .destroy = (void (*)(void*))Student_destroy,
        .greet = (void (*)(void*))Student_greet,
        .getAge = (int (*)(void*))Student_getAge,
        .study = (void (*)(void*))Student_study,
        .getGPA = (double (*)(void*))Student_getGPA
    };
    
    self->vtable = &vtable;
    self->school = strdup(school);
    self->gpa = gpa;
    
    // Initialize parent part
    self->parent_obj.name = strdup(name);
    self->parent_obj.age = age;
    self->parent_obj.parent_obj.type_name = "Student";
    
    return self;
}

// ====================== Additional Helper Macros ======================

// Safe casting between types
#define cast(type, obj) ((type*)(obj))

// Check if object is instance of class
#define instanceof(obj, class_name) (strcmp(obj->type_name, #class_name) == 0)

// Syntactic sugar for class methods
#define class_method(class_name, method_name) class_name##_##method_name

// ====================== Example Usage Function ======================

void example_usage() {
    // Create a base Object
    Object* obj = new(Object);
    char* obj_str = call(obj, toString);
    printf("%s\n", obj_str);
    free(obj_str);
    
    // Create a Person
    Person* person = new(Person, "John Doe", 30);
    char* person_str = call(person, toString);
    printf("%s\n", person_str);
    free(person_str);
    call(person, greet);
    
    // Create a Student
    Student* student = new(Student, "Jane Smith", 20, "Harvard", 3.9);
    char* student_str = call(student, toString);
    printf("%s\n", student_str);
    free(student_str);
    call(student, greet);
    call(student, study);
    printf("Student's GPA: %.2f\n", call(student, getGPA));
    
    // Clean up
    delete(obj);
    delete(person);
    delete(student);
}

// ====================== Enhanced Version with "self." syntax ======================

// This macro helps with the "self.x = y" syntax in constructor bodies
#define INIT_FIELD(obj, field, value) obj->field = value

// Simplified class definition with constructor body
#define CLASS_WITH_CONSTRUCTOR(name, constructor_args, constructor_body) \
    CLASS(name) \
    END_CLASS \
    CONSTRUCTOR(name, constructor_args) { \
        name* self = malloc(sizeof(name)); \
        if (!self) return NULL; \
        constructor_body \
        return self; \
    }

// Simplified method definition with body
#define METHOD_WITH_BODY(class_name, ret_type, name, args, body) \
    DEFINE_METHOD(class_name, ret_type, name, args) { \
        body \
    }

// Example usage for enhanced version
void enhanced_usage_example() {
    // This would be equivalent to:
    // class Person {
    //     constructor(name, age) {
    //         self.name = name;
    //         self.age = age;
    //     }
    //     greet() {
    //         printf("Hello, I'm %s\n", self.name);
    //     }
    // }
    
    Person* person = new(Person, "Alice", 25);
    call(person, greet);
    delete(person);
}

// ====================== Main Function ======================

#ifdef ENABLE_TESTS
int main() {
    printf("===== Class System Example =====\n");
    example_usage();
    printf("\n===== Enhanced Syntax Example =====\n");
    enhanced_usage_example();
    return 0;
}
#endif // ENABLE_TESTS

#endif // CLASS_SYSTEM_H

int main() {
    example_usage();
    enhanced_usage_example();
    return 0;
}
