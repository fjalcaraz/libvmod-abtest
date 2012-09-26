#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stddef.h>
#include <regex.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

#include "vmod_abtest.h"

#define ALLOC_CFG(cfg)								\
	(cfg) = malloc(sizeof(struct vmod_abtest));		\
	cfg_init((cfg));

struct rule {
	char* key;
	unsigned num;
	unsigned* probs;
	char** names;

	VTAILQ_ENTRY(rule) list;
};

#define VMOD_ABTEST_MAGIC 0xDD2914D8
struct vmod_abtest {
	unsigned magic;
	unsigned xid;
	VTAILQ_HEAD(, rule) rules;
};

static pthread_mutex_t cfg_mtx = PTHREAD_MUTEX_INITIALIZER;

static void cfg_init(struct vmod_abtest *cfg) {
	AN(cfg);
	cfg->magic = VMOD_ABTEST_MAGIC;
	VTAILQ_INIT(&cfg->rules);
}

static void cfg_clear(struct vmod_abtest *cfg) {
	CHECK_OBJ_NOTNULL(cfg, VMOD_ABTEST_MAGIC);

	struct rule *r, *r2;
	VTAILQ_FOREACH_SAFE(r, &cfg->rules, list, r2) {
		VTAILQ_REMOVE(&cfg->rules, r, list);
		free(r->key);
		free(r->probs);
		free(r->names);
		free(r);
	}
}

void cfg_free(struct vmod_abtest *cfg) {
	CHECK_OBJ_NOTNULL(cfg, VMOD_ABTEST_MAGIC);

	cfg_clear(cfg);
	cfg->xid = 0;
	cfg->magic = 0;
	free(cfg);
}

void wrt_rule(FILE *f, struct rule *rule) {
	AN(rule);
	fprintf(f, "%s:", rule->key);
}
void rd_rule(FILE *f, struct rule *rule) {}

/*
static struct rule ab_get_rule(struct vmod_abtest *cfg, const char *key) {
	struct rule *r;

	if (!key) {
		return NULL;
	}

	VTAILQ_FOREACH(r, &cfg->rules, list) {
		if (r->key && strcmp(r->key, key) == 0) {
			return r;
		}
	}
	return NULL;
}
*/

int init_function(struct vmod_priv *priv, const struct VCL_conf *conf) {
	priv->free  = (vmod_priv_free_f *)cfg_free;
	return (0);
}

#define RULE_REGEX "([[:alnum:]_]+):([[:digit:]]+);?"
static void parse_rule(struct rule *rule, const char *source) {
	unsigned n;
	int r;
	const char *s;
	char *name;
	regex_t regex;
	regmatch_t match[3];

/*
	if (rule->names != NULL) {
		for (num = 0; num < rule->num; num++) {
			if (rule->names[num] != NULL) {
				free(rule->names[num])
				rule->name = NULL;
			}
		}
		free(rule->names);
		rule->names = NULL;
	}
	if (rule->probs != null) {
		free rule->probs;
		rule->probs = NULL;
	}
*/

	if (regcomp(&regex, RULE_REGEX, REG_EXTENDED)){
		fprintf(stderr, "Could not compile regex\n");
		exit(1);
	}

	rule->num = 0;
	s = source;
	while ((r = regexec(&regex, s, 3, match, 0)) == 0) {
		rule->num++;
		fprintf(stderr, "parse: %s\n", s);
		s += match[0].rm_eo;
	}

	if (r != REG_NOMATCH) {
		char buf[100];
		regerror(r, &regex, buf, sizeof(buf));
		fprintf(stderr, "Regex match failed: %s\n", buf);
		exit(1);
	}

	fprintf(stderr, "num: %d\n", rule->num);
	rule->names = calloc(rule->num, sizeof(char*));
	rule->probs = calloc(rule->num, sizeof(unsigned));

	s = source;
	n = 0;
	while ((r = regexec(&regex, s, 3, match, 0)) == 0 && n < rule->num) {
		rule->names[n] = calloc(match[1].rm_eo - match[1].rm_so + 2, sizeof(char));
		strlcpy(rule->names[n], s + match[1].rm_so, match[1].rm_eo - match[1].rm_so + 1);

		rule->probs[n] = strtoul(s + match[2].rm_so, NULL, 10);

		s += match[0].rm_eo;
		n++;
	}

	fprintf(stderr, "Regex done\n");

	regfree(&regex);
}

void vmod_set_rule(struct sess *sp, struct vmod_priv *priv, const char *key, const char *rule) {
	fprintf(stderr, "set rule '%s': '%s'\n", key, rule);

	if (priv->priv == NULL) {
		ALLOC_CFG(priv->priv);
	}

	struct rule target;
	parse_rule(&target, rule);

	fprintf(stderr, "found %d entries in rule.\n", target.num);
	for (int i = 0; i < target.num; i++) {
		fprintf(stderr, "%d: %s -> %d\n", i, target.names[i], target.probs[i]);
	}

	exit(1);
}

void vmod_clear(struct sess *sp, struct vmod_priv *priv) {
	if (priv->priv == NULL) { return; }
	cfg_clear(priv->priv);
}

void vmod_load_config(struct sess *sp, struct vmod_priv *priv, const char *source) {
	AN(source);
	if (priv->priv == NULL) {
		ALLOC_CFG(priv->priv);
	}

	FILE *f;
	f = fopen(source, "r");
	AN(f);

	char buf[100];
	fgets(buf, sizeof buf, f);
	fprintf(stderr, "Read config from %s: %s\n", source, buf);

	//TODO: Read config

	AZ(fclose(f));
}

void vmod_save_config(struct sess *sp, struct vmod_priv *priv, const char *target) {
	AN(target);
	if (priv->priv == NULL) {
		return;
	}

	fprintf(stderr, "Save config to %s\n", target);

	FILE *f;
	f = fopen(target, "w");
	AN(f);
	(void)fprintf(f, "%s\n", "xav");
	AZ(fclose(f));
}

const char* vmod_get_version(struct sess *sp, struct vmod_priv *priv, const char *key) {
	if (priv->priv == NULL) {
		return NULL;
	}
}
