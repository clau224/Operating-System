#ifndef __LIB_KERNEL_LIST_H
#define __LIB_KERNEL_LIST_H

#include "global.h"

#define elemtoentry(struct_type, struct_member, member_ptr) \
	(struct_type*)((int)member_ptr - (int)(& ((struct_type*)0)->struct_member) )

struct list_elem{
	struct list_elem* prev;
	struct list_elem* next;
};

struct list{
	struct list_elem head;
	struct list_elem tail;
};

typedef bool (function)(struct list_elem*, int arg);

//初始化链表，即初始化头尾节点
void list_init(struct list*);

//将链表元素elem插入在元素before前
void list_insert_before(struct list_elem* before, struct list_elem* elem);

//添加元素到链表首
void list_push(struct list* l, struct list_elem* e);

//添加元素到链表尾
void list_append(struct list*l, struct list_elem* e);

//使元素e脱离链表
void list_remove(struct list_elem* e);

//从链表首弹出元素
struct list_elem* list_pop(struct list* l);

//从链表l中查找target元素
bool elem_find(struct list* l, struct list_elem* target);

//返回链表长度
uint32_t list_len(struct list* l);

//判断链表是否为空
bool list_empty(struct list* l);

//逐个判断是否满足条件，其中判定依据是func(elem, arg)
struct list_elem* list_traversal(struct list* l, function func, int arg);

#endif


