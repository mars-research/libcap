#include <stdio.h>
#include "../include/internal.h"
#include "../include/types.h"
#include <string.h>
#define ENOMEM 1

/* 
 * A testcase to check:
 * 1. Cspace initialization
 * 2. cptr cache initialization (User's stuff)
 * 3. Capability insertion in scpace
 * 4. Lookup after insertion
 * 5. Capability deletion 
 * 6. Capability Grant 
 * 7. Make code modular (Todo)
 * 8. Test with multiple threads (In process) 
 * 
 * For quick testing, I have embedded the code in one function.
 * I will subsequently break down the code in different functions.
 *
 */
int testcase1()
{
	int ret = 0;
	struct cspace * csp;
	struct cnode *cnode;
	cptr_t slot_out, slot_out_orig;
        struct cptr_cache *cache;
	char *p;
	struct cnode *check;
	struct cnode *check1;

	/* Initialize a cspace */
	csp = malloc(1 * sizeof(*csp));
	printf("\nTestCase : Cspace Initialization.\n");
	ret = __lcd_cap_init_cspace(csp);
	if (ret < 0)
		printf("Cspace Initialization Failed!!\n");
	else
		printf("Cspace Initialization Passed Address:%p \n", csp);

	/* cptr cache intialization. This is totally users stuff */
	ret = cptr_cache_init(&cache);
	
	ret = __klcd_alloc_cptr(cache, &slot_out);
        p = malloc(sizeof(char) * 4);
        if (!p) {
                LCD_ERR("alloc failed");
                ret = -ENOMEM;
                goto fail;
        }
	memset(p, 0, 4);

	/* Insert capability in cspace */
	printf("\nTestCase : Add Capability to Cspace.\n");
	ret = __lcd_cap_insert(csp, slot_out, p, LCD_CAP_TYPE_PAGE);

        if (ret) {
                LCD_ERR("cap insertion failed\n");
                goto fail;
        }

	/* Verification if capability is properly inserted in the cspace. */
	ret = __lcd_cnode_get(csp, slot_out, &check);
	if (ret < 0) {
		LCD_ERR("Lookup failed");
		goto fail;
	} else {
		if (check->object == p)
			printf("Capability Addition & Lookup Passed\n");
		else
			printf("Capability Addition & Lookup Failed!!!\n");
	}
	/* Release cnode Lock */
	__lcd_cnode_put(check);

	/* Capability deletion from cspace. 
	 */
	printf("\nTestCase : Delete Capability from Cspace.\n");
	__lcd_cap_delete(csp, slot_out);
	
	/*Lookup after deleting capability. It should Fail!!
	 */
	ret = __lcd_cnode_get(csp, slot_out, &check1);
        if (ret < 0) {
                LCD_ERR("Lookup failed\n");
		printf("Capability Deletion Passed\n");
        } else {
                if (check1->object == p)
                        printf("Screwed!!!\n");
                else
                        printf("Yippiee!!!\n");
        }
	/* Release cnode Lock */
	__lcd_cnode_put(check1);

	/* Free the cspace 
	 * Here we will destory the cspace.
	 * We will confirm the deletion after making a
	 * __lcd_cap_insert call. If the call fails, that means
	 * cspace has been deleted successfully.
	 */
	printf("\nTestCase : Delete Cspace.\n");
	__lcd_cap_destroy_cspace(csp);

	/* To check id cspace has been successfully destroyed,
	 * try to insert capability in cspace. Following call should
         * return error.
	 */
        ret = __lcd_cap_insert(csp, slot_out, p, LCD_CAP_TYPE_PAGE);

        if (ret < 0) {
		printf("Cspace Deletion Passed\n");
                goto fail;
        }
fail:	
	/* Free memory stuff. */
	return ret;
}

/*
 * Testcase to check the grant functionality
 */
int testcase_grant()
{
	int ret;
	struct cspace *scsp, *dcsp;
	struct cnode *cnode;
	cptr_t sslot, dslot;
	struct cptr_cache *scache, *dcache;
	char *p;
	struct cnode *scnode;
	struct cnode *dcnode;

	/* Initialize Source cspace */
	scsp = malloc(1 * sizeof(*scsp));
	ret = __lcd_cap_init_cspace(scsp);
	//ret = cspace_init(scsp);
	if (ret < 0) {
		printf("Cspace Setup Failed\n");
		return ret;
	}
	printf("Source Cspace Initilaized: Address=%p\n", scsp);

	/* cptr cache intialization. This is totally users stuff */
	ret = cptr_cache_init(&scache);
	if (ret < 0) {
		printf("Cache Initilization failed\n");
		goto fail1;
	}

	ret = __klcd_alloc_cptr(scache, &sslot);
	if (ret < 0) {
		printf("cptr allocation Failed!!\n");
		goto fail1;
	}
	p = malloc(sizeof(char) * 4);
	/* Insert capability in cspace */
	ret = __lcd_cap_insert(scsp, sslot, p, LCD_CAP_TYPE_PAGE);
	if (ret) {
		LCD_ERR("cap insertion failed\n");
		goto fail1;
	}
	printf("Added capability [%p] to Source cspace\n", p);

	/* Setup destination cspace */
	dcsp = malloc(1 * sizeof(*dcsp));
        ret = __lcd_cap_init_cspace(dcsp);
	if (ret < 0) {
		printf("Cspace Setup Failed\n");
		return ret;
	}
	printf("Destination Cspace Initilaized: Address=%p\n", dcsp);

	ret = cptr_cache_init(&dcache);
	if (ret < 0) {
		printf("Cache Initilization failed\n");
		goto fail2;
	}

	ret = __klcd_alloc_cptr(dcache, &dslot);
	if (ret < 0) {
		printf("cptr allocation Failed!!\n");
		goto fail2;
	}
	
	ret = libcap_grant_capability((void *)scsp, (void *)dcsp, sslot, dslot);
	if (ret < 0) {
		printf("Granting capability failed\n");
		goto fail2;
	}

	ret = __lcd_cnode_get(dcsp, dslot, &dcnode);
	if (ret < 0) {
		LCD_ERR("Lookup failed\n");
		goto fail2;
	} else {
		if (dcnode->object == p) {
			printf("Capability granted successfully from Cspace[%p] at slot 0x%lx \
			to Cspace[%p] at slot 0x%lx\n", scsp, cptr_val(sslot), dcsp, cptr_val(dslot));
		} else
			printf("Failed to grant capability!!\n");
	}
	/* Release cnode Lock */
	__lcd_cnode_put(dcnode);

fail2:
	__lcd_cap_destroy_cspace(dcsp);
fail1:
	__lcd_cap_destroy_cspace(scsp);
	return ret;
}

/*
 * capability insert
 */
int insert(struct cspace *csp, cptr_t slot)
{
	int ret = 0;
	void *p;

	p = malloc(1 * sizeof(*p));
	if (!p) {
		perror("malloc\n");
		ret = -1;
		goto fail;
	}

	ret = __lcd_cap_insert(csp, slot, p, LCD_CAP_TYPE_PAGE);
	if (ret < 0) {
		LCD_ERR("cap insertion failed\n");
	}

fail:
	return ret;
}

/*
 *Capability grant
 */
int grant(struct cspace *scsp, struct cspace *dcsp, cptr_t sslot, cptr_t dslot) {
	int ret = 0;

	ret = libcap_grant_capability((void *)scsp, (void *)dcsp, sslot, dslot);
	if (ret < 0)
		printf("Granting capability failed\n");

	return ret;
}

/*
 *Get Cnode
 */
int get_cnode(struct cspace *csp, cptr_t sslot) {
	int ret = 0;
	struct cnode *dcnode;

	ret = __lcd_cnode_get(csp, sslot, &dcnode);
	if (ret < 0)
		LCD_ERR("Destination CSPACE Lookup failed\n");
	/* Release cnode Lock */
	__lcd_cnode_put(dcnode);
	return ret;
}

/* 
 * Capability Revoke
 */
int revoke(struct cspace *csp, cptr_t sslot, struct cptr_cache *scache) {
	int ret = 0;

	ret = __lcd_cap_revoke(csp, sslot);
	if (ret < 0)
		printf("Revoke failed\n");
	__klcd_free_cptr(scache, sslot);

	return ret;
}

/*
 * This testcase is checking capability revoke function.
 * Here a capability is inserted in CSPACE A and granted to CSPACE B.
 * Capability is then revoked from CSPACE A.
 * Check is performed if Capability is still present in CSPACE B.
 * It should not be present in CSPACE B.
 */
int testcase_revoke() {
	int ret = 0;
	struct cspace *scsp, *dcsp;
	struct cptr_cache *scache, *dcache;
	cptr_t sslot, dslot;

	printf("\nTestcase : Capability Revocation\n");
	/* 1st CSPACE */
	scsp = malloc(1 * sizeof(*scsp));
        if (!scsp) {
                perror("malloc cspace\n");
                exit(1);
        }
        ret = __lcd_cap_init_cspace(scsp);
        if (ret < 0) {
                printf("Cspace Initialization failed\n");
                goto fail1;
        }
        ret = cptr_cache_init(&scache);
        if (ret < 0) {
                printf("cptr cache Initialization failed\n");
                goto fail1;
        }

	/* 2nd CSPACE */
        dcsp = malloc(1 * sizeof(*dcsp));
        if (!dcsp) {
                perror("malloc cspace\n");
                goto fail1;
        }
        ret = __lcd_cap_init_cspace(dcsp);
        if (ret < 0) {
                printf("Cspace Initialization failed\n");
                goto fail2;
        }
        ret = cptr_cache_init(&dcache);
        if (ret < 0) {
                printf("cptr cache Initialization failed\n");
                goto fail2;
        }

	ret = __klcd_alloc_cptr(scache, &sslot);
        if (ret < 0) {
                printf("cptr aloocation failed\n");
                goto fail;
        }
	ret = __klcd_alloc_cptr(dcache, &dslot);
	if (ret < 0) {
                printf("cptr aloocation failed\n");
		goto fail;
	}

	ret = insert(scsp, sslot);
	if (ret < 0)
		goto fail2;
	ret = grant(scsp, dcsp, sslot, dslot);
	if (ret < 0)
		goto fail2;
	ret = revoke(scsp, sslot, scache);
	if (ret < 0)
		goto fail2;
	ret = get_cnode(dcsp, dslot);
	if (ret < 0) {
		printf("\nTestcase Capability Revocation Passed\n");
		goto fail2;
	}
	printf("\nTestcase capability Revocation Failed\n");

fail2:
	__lcd_cap_destroy_cspace(dcsp);
fail1:
	__lcd_cap_destroy_cspace(scsp);
fail:
	return ret;
}

int main()
{
	int ret = 0;

	ret = testcase1();
	ret = testcase_grant();
	ret = testcase_revoke();

	return ret;
}
