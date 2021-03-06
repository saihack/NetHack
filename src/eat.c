/*	this file has been modified by saihack, 21.06.2013	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
/*#define DEBUG		/* uncomment to enable new eat code debugging */

#ifdef DEBUG
# ifdef WIZARD
#define debugpline	if (wizard) pline
# else
#define debugpline	pline
# endif
#endif

STATIC_PTR int NDECL(eatmdone);
STATIC_PTR int NDECL(eatfood);
STATIC_PTR int NDECL(opentin);
STATIC_PTR int NDECL(unfaint);

#ifdef OVLB
static void FDECL(choke, (struct obj *));
static void NDECL(recalc_wt);
static struct obj *FDECL(touchfood, (struct obj *));
static void NDECL(do_reset_eat);
static void FDECL(done_eating, (BOOLEAN_P));
static void FDECL(cprefx, (int));
static int FDECL(intrinsic_possible, (int,struct permonst *));
static void FDECL(givit, (int,struct permonst *));
static void FDECL(cpostfx, (int));
static void FDECL(start_tin, (struct obj *));
static int FDECL(eatcorpse, (struct obj *));
static void FDECL(start_eating, (struct obj *));
static void FDECL(fprefx, (struct obj *));
static void FDECL(fpostfx, (struct obj *));
static int NDECL(bite);

#ifdef POLYSELF
static int FDECL(rottenfood, (struct obj *));
static void NDECL(eatspecial);
static void FDECL(eatring, (struct obj *));
static const char * FDECL(foodword, (struct obj *));
#else
static int NDECL(rottenfood);
#endif /* POLYSELF */

char corpsename[60];
char msgbuf[BUFSZ];

#endif /* OVLB */

/* hunger texts used on bottom line (each 8 chars long) */
#define SATIATED	0
#define NOT_HUNGRY	1
#define HUNGRY		2
#define WEAK		3
#define FAINTING	4
#define FAINTED		5
#define STARVED		6

#ifdef OVLB

const char *hu_stat[] = {
	"Satiated",
	"        ",
	"Hungry  ",
	"Weak    ",
	"Fainting",
	"Fainted ",
	"Starved "
};

#endif /* OVLB */

#ifndef OVLB

STATIC_DCL NEARDATA const char comestibles[];
#ifdef POLYSELF
STATIC_OVL NEARDATA const char allobj[];
#endif /* POLYSELF */

#else

STATIC_OVL NEARDATA const char comestibles[] = { FOOD_CLASS, 0 };

#ifdef POLYSELF
/* Gold must come first for getobj(). */
STATIC_OVL NEARDATA const char allobj[] = {
	GOLD_CLASS, WEAPON_CLASS, ARMOR_CLASS, POTION_CLASS, SCROLL_CLASS,
	WAND_CLASS, RING_CLASS, AMULET_CLASS, FOOD_CLASS, TOOL_CLASS,
	GEM_CLASS, ROCK_CLASS, BALL_CLASS, CHAIN_CLASS, SPBOOK_CLASS, 0 };
#endif /* POLYSELF */

#endif /* OVLB */
#ifdef OVL1
# ifdef POLYSELF

boolean
is_edible(obj)
register struct obj *obj;
{
	if (metallivorous(uasmon) && is_metallic(obj))
		return TRUE;
	if (u.umonnum == PM_GELATINOUS_CUBE && is_organic(obj))
		return TRUE;
	return((boolean)(!!index(comestibles, obj->oclass)));
}
# endif /* POLYSELF */
#endif /* OVL1 */
#ifdef OVLB

void
init_uhunger()
{
	u.uhunger = 900;
	u.uhs = NOT_HUNGRY;
}

static const struct { const char *txt; int nut; } tintxts[] = {
	{"deep fried",	 60},
	{"pickled",	 40},
	{"soup made from", 20},
	{"pureed",	500},
	{"rotten",	-50},
	{"homemade",	 50},
	{"", 0}
};
#define	TTSZ	SIZE(tintxts)

static NEARDATA struct {
	struct	obj *tin;
	int	usedtime, reqtime;
} tin;

static NEARDATA struct {
	struct	obj *piece;	/* the thing being eaten, or last thing that
				 * was partially eaten, unless that thing was
				 * a tin, which uses the tin structure above */
	int	usedtime,	/* turns spent eating */
		reqtime;	/* turns required to eat */
	int	nmod;		/* coded nutrition per turn */
	Bitfield(canchoke,1);	/* was satiated at beginning */
	Bitfield(fullwarn,1);	/* have warned about being full */
	Bitfield(eating,1);	/* victual currently being eaten */
	Bitfield(doreset,1);	/* stop eating at end of turn */
} victual;

STATIC_PTR
int
eatmdone()		/* called after mimicing is over */
{
	u.usym =
#ifdef POLYSELF
		u.mtimedone ? uasmon->mlet :
#endif
		S_HUMAN;
	newsym(u.ux,u.uy);
	return 0;
}

/* Created by GAN 01/28/87
 * Amended by AKP 09/22/87: if not hard, don't choke, just vomit.
 * Amended by 3.  06/12/89: if not hard, sometimes choke anyway, to keep risk.
 *		  11/10/89: if hard, rarely vomit anyway, for slim chance.
 */
/*ARGSUSED*/
static void
choke(food)	/* To a full belly all food is bad. (It.) */
	register struct obj *food;
{
	/* only happens if you were satiated */
	if(u.uhs != SATIATED) return;

	if (pl_character[0] == 'K' && u.ualign.type == A_LAWFUL)
		u.ualign.record--;	/* gluttony is unchivalrous */

	exercise(A_CON, FALSE);

	if (!rn2(20) || Breathless) {
		You("stuff yourself and then vomit voluminously.");
		morehungry(1000);	/* you just got *very* sick! */
		vomit();
	} else {
		killer_format = KILLED_BY_AN;
		/*
		 * Note all "killer"s below read "Choked on %s" on the
		 * high score list & tombstone.  So plan accordingly.
		 */
		if(food) {
#ifdef POLYSELF
			You("choke over your %s.", foodword(food));
			if (food->oclass == GOLD_CLASS) {
				killer_format = KILLED_BY;
				killer = "eating too rich a meal";
			} else {
#else
				You("choke over your food.");
#endif
				killer = singular(food, xname);
#ifdef POLYSELF
			}
#endif
		} else {
			You("choke over it.");
			killer = "quick snack";
		}
		You("die...");
		done(CHOKING);
	}
}

static void
recalc_wt()	/* modify object wt. depending on time spent consuming it */
{
	register struct obj *piece = victual.piece;

#ifdef DEBUG
	debugpline("Old weight = %d", piece->owt);
	debugpline("Used time = %d, Req'd time = %d",
		victual.usedtime, victual.reqtime);
#endif
	/* weight(piece) = weight of full item */
	if(victual.usedtime)
	    piece->owt = eaten_stat(weight(piece), piece);
#ifdef DEBUG
	debugpline("New weight = %d", piece->owt);
#endif
}

void
reset_eat()		/* called when eating interrupted by an event */
{
    /* we only set a flag here - the actual reset process is done after
     * the round is spent eating.
     */
	if(victual.eating && !victual.doreset) {
#ifdef DEBUG
	    debugpline("reset_eat...");
#endif
	    victual.doreset = TRUE;
	}
	return;
}

void
bill_dummy_object(otmp)
register struct obj *otmp;
{
	/* Create a dummy duplicate to put on bill.  The duplicate exists
	 * only in the billobjs chain.  This function is used when a store
	 * object is being altered, and a copy of the original is needed
	 * for billing purposes.
	 */
	register struct obj *dummy;

	if(otmp->unpaid)
		subfrombill(otmp, shop_keeper(*u.ushops));
	dummy = newobj(otmp->onamelth);
	*dummy = *otmp;
	dummy->o_id = flags.ident++;
	dummy->owt = weight(dummy);
	if(otmp->onamelth)
		(void)strncpy(ONAME(dummy),ONAME(otmp), (int)otmp->onamelth);
	if(Is_candle(dummy)) dummy->lamplit = 0;
	addtobill(dummy, FALSE, TRUE, TRUE);
}

static struct obj *
touchfood(otmp)
register struct obj *otmp;
{
	if (otmp->quan > 1L) {
	    if(!carried(otmp))
		(void) splitobj(otmp, 1L);
	    else
		otmp = splitobj(otmp, otmp->quan - 1L);
#ifdef DEBUG
	    debugpline("split object,");
#endif
	}

	if (!otmp->oeaten) {
	    if(((!carried(otmp) && costly_spot(otmp->ox, otmp->oy) &&
		 !otmp->no_charge)
		 || otmp->unpaid) &&
		 (otmp->otyp == CORPSE || objects[otmp->otyp].oc_delay > 1)) {
		/* create a dummy duplicate to put on bill */
		verbalize("You bit it, you bought it!");
		bill_dummy_object(otmp);
		otmp->no_charge = 1;	/* you now own this */
	    }
	    otmp->oeaten = (otmp->otyp == CORPSE ?
				(int)mons[otmp->corpsenm].cnutrit :
				objects[otmp->otyp].oc_nutrition);
	}

	if (carried(otmp)) {
	    freeinv(otmp);
	    if(inv_cnt() >= 52)
		dropy(otmp);
	    else
		otmp = addinv(otmp); /* unlikely but a merge is possible */
	}
	return(otmp);
}

/* When food decays, in the middle of your meal, we don't want to dereference
 * any dangling pointers, so set it to null (which should still trigger
 * do_reset_eat() at the beginning of eatfood()) and check for null pointers
 * in do_reset_eat().
 */
void
food_disappears(obj)
register struct obj *obj;
{
	if (obj == victual.piece) victual.piece = (struct obj *)0;
}

static void
do_reset_eat() {

#ifdef DEBUG
	debugpline("do_reset_eat...");
#endif
	if (victual.piece) {
		victual.piece = touchfood(victual.piece);
		recalc_wt();
	}
	victual.fullwarn = victual.eating = victual.doreset = FALSE;
	/* Do not set canchoke to FALSE; if we continue eating the same object
	 * we need to know if canchoke was set when they started eating it the
	 * previous time.  And if we don't continue eating the same object
	 * canchoke always gets recalculated anyway.
	 */
	stop_occupation();
}

STATIC_PTR
int
eatfood()		/* called each move during eating process */
{
	if(!carried(victual.piece) && !obj_here(victual.piece, u.ux, u.uy)) {
		/* maybe it was stolen? */
		do_reset_eat();
		return(0);
	}
	if(!victual.eating) return(0);

	if(++victual.usedtime <= victual.reqtime) {
	    if(bite()) return(0);
	    return(1);	/* still busy */
	} else {	/* done */
	    done_eating(TRUE);
	    return(0);
	}
}

static void
done_eating(message)
boolean message;
{
#ifndef NO_SIGNAL
	victual.piece->in_use = TRUE;
#endif
	if (nomovemsg) {
		if (message) pline("%s",nomovemsg);
		nomovemsg = 0;
	} else if (message)
		You("finish eating %s.", the(singular(victual.piece, xname)));

	if(victual.piece->otyp == CORPSE)
		cpostfx(victual.piece->corpsenm);
	else
		fpostfx(victual.piece);

	if (carried(victual.piece)) useup(victual.piece);
	else useupf(victual.piece);
	victual.piece = (struct obj *) 0;
	victual.fullwarn = victual.eating = victual.doreset = FALSE;
}

static void
cprefx(pm)
register int pm;
{
	if ((pl_character[0]=='E') ? is_elf(&mons[pm]) : is_human(&mons[pm])) {
#ifdef POLYSELF
		if (uasmon != &playermon) {
			You("have a bad feeling deep inside.");
		}
#endif /* POLYSELF */
		You("cannibal!  You will regret this!");
		Aggravate_monster |= FROMOUTSIDE;
	}

	switch(pm) {
	    case PM_LITTLE_DOG:
	    case PM_DOG:
	    case PM_LARGE_DOG:
	    case PM_KITTEN:
	    case PM_HOUSECAT:
	    case PM_LARGE_CAT:
		You("feel that eating the %s was a bad idea.", mons[pm].mname);
		Aggravate_monster |= FROMOUTSIDE;
		break;
	    case PM_COCKATRICE:
	    case PM_MEDUSA:
#ifdef POLYSELF
		if(!resists_ston(uasmon))
		    if(!(poly_when_stoned(uasmon) && polymon(PM_STONE_GOLEM)))
#endif
			{
			char *cruft;	/* killer is const char * */
			killer_format = KILLED_BY;
			killer = cruft = (char *)  /* sizeof "s" includes \0 */
					alloc((unsigned) strlen(mons[pm].mname)
						+ sizeof " meat");
			Sprintf(cruft, "%s meat", mons[pm].mname);
			You("turn to stone.");
			done(STONING);
			}
			break;
	    case PM_LIZARD:
		/* Relief from cockatrices -dgk */
		if (Stoned) {
			Stoned = 0;
			You("feel limber!");
		}
		break;
	    case PM_DEATH:
	    case PM_PESTILENCE:
	    case PM_FAMINE:
		{ char buf[BUFSZ];
		    pline("Eating that is instantly fatal.");
		    Sprintf(buf, "unwisely ate the body of %s",
			    mons[pm].mname);
		    killer = buf;
		    killer_format = NO_KILLER_PREFIX;
		    done(DIED);
		    /* It so happens that since we know these monsters */
		    /* cannot appear in tins, victual.piece will always */
		    /* be what we want, which is not generally true. */
		    revive_corpse(victual.piece, 0, FALSE);
		    return;
		}
	    default:
		if(acidic(&mons[pm]) && Stoned) {
		    pline("What a pity - you just destroyed a future piece of art!");
		    Stoned = 0;
		}
	}
	return;
}


/*
 * If you add an intrinsic that can be gotten by eating a monster, add it
 * to intrinsic_possible() and givit().  (It must already be in prop.h to
 * be an intrinsic property.)
 * It would be very easy to make the intrinsics not try to give you one
 * that you already had by checking to see if you have it in
 * intrinsic_possible() instead of givit().
 */

/* intrinsic_possible() returns TRUE iff a monster can give an intrinsic. */
static int
intrinsic_possible(type, ptr)
int type;
register struct permonst *ptr;
{
	switch (type) {
	    case FIRE_RES:
#ifdef DEBUG
		if (ptr->mconveys & MR_FIRE) {
			debugpline("can get fire resistance");
			return(TRUE);
		} else  return(FALSE);
#else
		return(ptr->mconveys & MR_FIRE);
#endif
	    case SLEEP_RES:
#ifdef DEBUG
		if (ptr->mconveys & MR_SLEEP) {
			debugpline("can get sleep resistance");
			return(TRUE);
		} else  return(FALSE);
#else
		return(ptr->mconveys & MR_SLEEP);
#endif
	    case COLD_RES:
#ifdef DEBUG
		if (ptr->mconveys & MR_COLD) {
			debugpline("can get cold resistance");
			return(TRUE);
		} else  return(FALSE);
#else
		return(ptr->mconveys & MR_COLD);
#endif
	    case DISINT_RES:
#ifdef DEBUG
		if (ptr->mconveys & MR_DISINT) {
			debugpline("can get disintegration resistance");
			return(TRUE);
		} else  return(FALSE);
#else
		return(ptr->mconveys & MR_DISINT);
#endif
	    case SHOCK_RES:	/* shock (electricity) resistance */
#ifdef DEBUG
		if (ptr->mconveys & MR_ELEC) {
			debugpline("can get shock resistance");
			return(TRUE);
		} else  return(FALSE);
#else
		return(ptr->mconveys & MR_ELEC);
#endif
	    case POISON_RES:
#ifdef DEBUG
		if (ptr->mconveys & MR_POISON) {
			debugpline("can get poison resistance");
			return(TRUE);
		} else  return(FALSE);
#else
		return(ptr->mconveys & MR_POISON);
#endif
	    case TELEPORT:
#ifdef DEBUG
		if (can_teleport(ptr)) {
			debugpline("can get teleport");
			return(TRUE);
		} else  return(FALSE);
#else
		return(can_teleport(ptr));
#endif
	    case TELEPORT_CONTROL:
#ifdef DEBUG
		if (control_teleport(ptr)) {
			debugpline("can get teleport control");
			return(TRUE);
		} else  return(FALSE);
#else
		return(control_teleport(ptr));
#endif
	    case TELEPAT:
#ifdef DEBUG
		if (telepathic(ptr)) {
			debugpline("can get telepathy");
			return(TRUE);
		} else  return(FALSE);
#else
		return(telepathic(ptr));
#endif
	    default:
		return(FALSE);
	}
	/*NOTREACHED*/
}

/* givit() tries to give you an intrinsic based on the monster's level
 * and what type of intrinsic it is trying to give you.
 */
static void
givit(type, ptr)
int type;
register struct permonst *ptr;
{
	register int chance;

#ifdef DEBUG
	debugpline("Attempting to give intrinsic %d", type);
#endif
	/* some intrinsics are easier to get than others */
	switch (type) {
		case POISON_RES:
			if ((ptr == &mons[PM_KILLER_BEE] ||
					ptr == &mons[PM_SCORPION]) && !rn2(4))
				chance = 1;
			else
				chance = 15;
			break;
		case TELEPORT:
			chance = 10;
			break;
		case TELEPORT_CONTROL:
			chance = 12;
			break;
		case TELEPAT:
			chance = 1;
			break;
		default:
			chance = 15;
			break;
	}

	if (ptr->mlevel <= rn2(chance))
		return;		/* failed die roll */

	switch (type) {
	    case FIRE_RES:
#ifdef DEBUG
		debugpline("Trying to give fire resistance");
#endif
		if(!(HFire_resistance & FROMOUTSIDE)) {
			You(Hallucination ? "be chillin'." :
			    "feel a momentary chill.");
			HFire_resistance |= FROMOUTSIDE;
		}
		break;
	    case SLEEP_RES:
#ifdef DEBUG
		debugpline("Trying to give sleep resistance");
#endif
		if(!(HSleep_resistance & FROMOUTSIDE)) {
			You("feel wide awake.");
			HSleep_resistance |= FROMOUTSIDE;
		}
		break;
	    case COLD_RES:
#ifdef DEBUG
		debugpline("Trying to give cold resistance");
#endif
		if(!(HCold_resistance & FROMOUTSIDE)) {
			You("feel full of hot air.");
			HCold_resistance |= FROMOUTSIDE;
		}
		break;
	    case DISINT_RES:
#ifdef DEBUG
		debugpline("Trying to give disintegration resistance");
#endif
		if(!(HDisint_resistance & FROMOUTSIDE)) {
			You(Hallucination ?
			    "feel totally together, man." :
			    "feel very firm.");
			HDisint_resistance |= FROMOUTSIDE;
		}
		break;
	    case SHOCK_RES:	/* shock (electricity) resistance */
#ifdef DEBUG
		debugpline("Trying to give shock resistance");
#endif
		if(!(HShock_resistance & FROMOUTSIDE)) {
			if (Hallucination)
				You("feel grounded in reality.");
			else
				Your("health currently feels amplified!");
			HShock_resistance |= FROMOUTSIDE;
		}
		break;
	    case POISON_RES:
#ifdef DEBUG
		debugpline("Trying to give poison resistance");
#endif
		if(!(HPoison_resistance & FROMOUTSIDE)) {
			You("feel healthy.");
			HPoison_resistance |= FROMOUTSIDE;
		}
		break;
	    case TELEPORT:
#ifdef DEBUG
		debugpline("Trying to give teleport");
#endif
		if(!(HTeleportation & FROMOUTSIDE)) {
			You(Hallucination ? "feel diffuse." :
			    "feel very jumpy.");
			HTeleportation |= FROMOUTSIDE;
		}
		break;
	    case TELEPORT_CONTROL:
#ifdef DEBUG
		debugpline("Trying to give teleport control");
#endif
		if(!(HTeleport_control & FROMOUTSIDE)) {
			You(Hallucination ?
			    "feel centered in your personal space." :
			    "feel in control of yourself.");
			HTeleport_control |= FROMOUTSIDE;
		}
		break;
	    case TELEPAT:
#ifdef DEBUG
		debugpline("Trying to give telepathy");
#endif
		if(!(HTelepat & FROMOUTSIDE)) {
			You(Hallucination ?
			    "feel in touch with the cosmos." :
			    "feel a strange mental acuity.");
			HTelepat |= FROMOUTSIDE;
			/* If blind, make sure monsters show up. */
			if (Blind) see_monsters();
		}
		break;
	    default:
#ifdef DEBUG
		debugpline("Tried to give an impossible intrinsic");
#endif
		break;
	}
}

static void
cpostfx(pm)		/* called after completely consuming a corpse */
register int pm;
{
	register int tmp = 0;

	switch(pm) {
	    case PM_WRAITH:
		pluslvl();
		break;
#ifdef POLYSELF
	    case PM_HUMAN_WERERAT:
		u.ulycn = PM_WERERAT;
		break;
	    case PM_HUMAN_WEREJACKAL:
		u.ulycn = PM_WEREJACKAL;
		break;
	    case PM_HUMAN_WEREWOLF:
		u.ulycn = PM_WEREWOLF;
		break;
#endif
	    case PM_NURSE:
		u.uhp = u.uhpmax;
		flags.botl = 1;
		break;
	    case PM_STALKER:
		if(!Invis) {
			HInvis = rn1(100, 50);
			if(!See_invisible)
				newsym(u.ux, u.uy);
		} else {
			register long oldprop = See_invisible;
			if (!(HInvis & INTRINSIC)) You("feel hidden!");
			HInvis |= FROMOUTSIDE;
			HSee_invisible |= FROMOUTSIDE;
			if (!oldprop)
				newsym(u.ux, u.uy);
		}
		/* fall into next case */
	    case PM_YELLOW_LIGHT:
		/* fall into next case */
	    case PM_GIANT_BAT:
		make_stunned(HStun + 30,FALSE);
		/* fall into next case */
	    case PM_BAT:
		make_stunned(HStun + 30,FALSE);
		break;
	    case PM_GIANT_MIMIC:
		tmp += 10;
		/* fall into next case */
	    case PM_LARGE_MIMIC:
		tmp += 20;
		/* fall into next case */
	    case PM_SMALL_MIMIC:
		tmp += 20;
		if(u.usym == S_HUMAN) {
		    You("can't resist the temptation to mimic a pile of gold.");
		    nomul(-tmp);
		    afternmv = eatmdone;
		    if (pl_character[0]=='E')
			nomovemsg = "You now prefer mimicking an elf again.";
		    else
			nomovemsg = "You now prefer mimicking a human again.";
		    u.usym = 0; /* hack! no monster sym 0; use for gold */
		    newsym(u.ux,u.uy);
		}
		break;
	    case PM_QUANTUM_MECHANIC:
		Your("velocity suddenly seems very uncertain!");
		if (Fast & INTRINSIC) {
			Fast &= ~INTRINSIC;
			You("seem slower.");
		} else {
			Fast |= FROMOUTSIDE;
			You("seem faster.");
		}
		break;
	    case PM_LIZARD:
		if (HStun > 2)  make_stunned(2L,FALSE);
		if (HConfusion > 2)  make_confused(2L,FALSE);
		break;
	    case PM_CHAMELEON:
		You("feel a change coming over you.");
#ifdef POLYSELF
		polyself();
#else
		newman();
#endif
		break;
	    case PM_MIND_FLAYER:
		if (ABASE(A_INT) < ATTRMAX(A_INT)) {
			if (!rn2(2)) {
				pline("Yum! That was real brain food!");
				(void) adjattrib(A_INT, 1, FALSE);
				break;	/* don't give them telepathy, too */
			}
		}
		else {
			pline("For some reason, that tasted bland.");
		}
		/* fall through to default case */
	    default: {
		register struct permonst *ptr = &mons[pm];
		int i, count;

		if(dmgtype(ptr, AD_STUN) || pm==PM_VIOLET_FUNGUS) {
			pline ("Oh wow!  Great stuff!");
			make_hallucinated(HHallucination + 200,FALSE,0L);
		}
		/* prevent polymorph abuse by killing/eating your offspring */
		if(pm >= PM_BABY_GRAY_DRAGON && pm <= PM_BABY_YELLOW_DRAGON)
			return;
		if(is_giant(ptr)) gainstr((struct obj *)0, 0);

		/* Check the monster for all of the intrinsics.  If this
		 * monster can give more than one, pick one to try to give
		 * from among all it can give.
		 *
		 * If a monster can give 4 intrinsics then you have
		 * a 1/1 * 1/2 * 2/3 * 3/4 = 1/4 chance of getting the first,
		 * a 1/2 * 2/3 * 3/4 = 1/4 chance of getting the second,
		 * a 1/3 * 3/4 = 1/4 chance of getting the third,
		 * and a 1/4 chance of getting the fourth.
		 *
		 * And now a proof by induction:
		 * it works for 1 intrinsic (1 in 1 of getting it)
		 * for 2 you have a 1 in 2 chance of getting the second,
		 *	otherwise you keep the first
		 * for 3 you have a 1 in 3 chance of getting the third,
		 *	otherwise you keep the first or the second
		 * for n+1 you have a 1 in n+1 chance of getting the (n+1)st,
		 *	otherwise you keep the previous one.
		 * Elliott Kleinrock, October 5, 1990
		 */

		 count = 0;	/* number of possible intrinsics */
		 tmp = 0;	/* which one we will try to give */
		 for (i = 1; i <= LAST_PROP; i++) {
			if (intrinsic_possible(i, ptr)) {
				count++;
				/* a 1 in count chance of replacing the old
				 * one with this one, and a count-1 in count
				 * chance of keeping the old one.  (note
				 * that 1 in 1 and 0 in 1 are what we want
				 * for the first one
				 */
				if (!rn2(count)) {
#ifdef DEBUG
					debugpline("Intrinsic %d replacing %d",
								i, tmp);
#endif
					tmp = i;
				}
			}
		 }

		 /* if any found try to give them one */
		 if (count) givit(tmp, ptr);
	    }
	    break;
	}
	return;
}

STATIC_PTR
int
opentin()		/* called during each move whilst opening a tin */
{
	register int r;

	if(!carried(tin.tin) && !obj_here(tin.tin, u.ux, u.uy))
					/* perhaps it was stolen? */
		return(0);		/* %% probably we should use tinoid */
	if(tin.usedtime++ >= 50) {
		You("give up your attempt to open the tin.");
		return(0);
	}
	if(tin.usedtime < tin.reqtime)
		return(1);		/* still busy */
	if(tin.tin->otrapped || 
	   (tin.tin->cursed && tin.tin->spe != -1 && !rn2(8))) {
		b_trapped("tin", 0);
		goto use_me;
	}
	You("succeed in opening the tin.");
	if(tin.tin->spe != 1) {
	    if(tin.tin->corpsenm == -1) {
		pline("It turns out to be empty.");
		tin.tin->dknown = tin.tin->known = TRUE;
		goto use_me;
	    }
	    r = tin.tin->cursed ? 4 :		/* Always rotten if cursed */
		    (tin.tin->spe == -1) ? 5 :	/* "homemade" if player made */
			rn2(TTSZ-1);		/* else take your pick */
	    if (tin.tin->spe == -1 && !tin.tin->blessed && !rn2(7))
		r = 4;				/* some homemade tins go bad */
	    pline("It smells like %s.", makeplural(
		  Hallucination ? rndmonnam() : mons[tin.tin->corpsenm].mname));
	    if (yn("Eat it?") == 'n') {
		if (!Hallucination) tin.tin->dknown = tin.tin->known = TRUE;
		if (flags.verbose) You("discard the open tin.");
		goto use_me;
	    }
	    /* in case stop_occupation() was called on previous meal */
	    victual.piece = (struct obj *)0;
	    victual.fullwarn = victual.eating = victual.doreset = FALSE;

	    You("consume %s %s.", tintxts[r].txt,
			mons[tin.tin->corpsenm].mname);
	    tin.tin->dknown = tin.tin->known = TRUE;
	    cprefx(tin.tin->corpsenm); cpostfx(tin.tin->corpsenm);

	    /* check for vomiting added by GAN 01/16/87 */
	    if(tintxts[r].nut < 0) make_vomiting((long)rn1(15,10), FALSE);
	    else lesshungry(tintxts[r].nut);

	    if(r == 0) {			/* Deep Fried */
	        /* Assume !Glib, because you can't open tins when Glib. */
		Glib += rnd(15);
		pline("Eating deep fried food made your %s very slippery.",
		      makeplural(body_part(FINGER)));
	    }
	} else {
	    if (tin.tin->cursed)
		pline("It contains some decaying %s substance.",
			Hallucination ? hcolor() : green);
	    else
		pline("It contains spinach.");

	    if (yn("Eat it?") == 'n') {
		if (!Hallucination && !tin.tin->cursed)
		    tin.tin->dknown = tin.tin->known = TRUE;
		if (flags.verbose)
		    You("discard the open tin.");
		goto use_me;
	    }
	    if (!tin.tin->cursed)
		pline("This makes you feel like %s!",
		      Hallucination ? "Swee'pea" : "Popeye");
	    lesshungry(600);
	    gainstr(tin.tin, 0);
	}
	tin.tin->dknown = tin.tin->known = TRUE;
use_me:
	if (carried(tin.tin)) useup(tin.tin);
	else useupf(tin.tin);
	tin.tin = (struct obj *) 0;
	return(0);
}

static void
start_tin(otmp)		/* called when starting to open a tin */
	register struct obj *otmp;
{
	register int tmp;

#ifdef POLYSELF
	if (metallivorous(uasmon)) {
		You("bite right into the metal tin...");
		tmp = 1;
	} else if (nolimbs(uasmon)) {
		You("cannot handle the tin properly to open it.");
		return;
	} else
#endif
	if (otmp->blessed) {
		pline("The tin opens like magic!");
		tmp = 1;
	} else if(uwep) {
		switch(uwep->otyp) {
		case TIN_OPENER:
			tmp = 1;
			break;
		case DAGGER:
		case ELVEN_DAGGER:
		case ORCISH_DAGGER:
		case ATHAME:
		case CRYSKNIFE:
			tmp = 3;
			break;
		case PICK_AXE:
		case AXE:
			tmp = 6;
			break;
		default:
			goto no_opener;
		}
		pline("Using your %s you try to open the tin.",
			aobjnam(uwep, NULL));
	} else {
no_opener:
		pline("It is not so easy to open this tin.");
		if(Glib) {
			pline("The tin slips from your %s.",
			      makeplural(body_part(FINGER)));
			if(otmp->quan > 1L) {
				register struct obj *obj;
				obj = splitobj(otmp, 1L);
				if(otmp == uwep) setuwep(obj);
			}
			if (carried(otmp)) dropx(otmp);
			else stackobj(otmp);
			return;
		}
		tmp = rn1(1 + 500/((int)(ACURR(A_DEX) + ACURRSTR)), 10);
	}
	tin.reqtime = tmp;
	tin.usedtime = 0;
	tin.tin = otmp;
	set_occupation(opentin, "opening the tin", 0);
	return;
}

int
Hear_again()		/* called when waking up after fainting */
{
	flags.soundok = 1;
	return 0;
}

static int
#ifdef POLYSELF
rottenfood(obj)
struct obj *obj;
#else
rottenfood()
#endif
{		/* called on the "first bite" of rotten food */
#ifdef POLYSELF
	pline("Blecch!  Rotten %s!", foodword(obj));
#else
	pline("Blecch!  Rotten food!");
#endif
	if(!rn2(4)) {
		if (Hallucination) You("feel rather trippy.");
		else You("feel rather %s.", body_part(LIGHT_HEADED));
		make_confused(HConfusion + d(2,4),FALSE);
	} else if(!rn2(4) && !Blind) {
		pline("Everything suddenly goes dark.");
		make_blinded((long)d(2,10),FALSE);
	} else if(!rn2(3)) {
		const char *what, *where;
		if (!Blind)
		    what = "goes",  where = "dark";
		else if (Levitation || Is_airlevel(&u.uz) ||
			 Is_waterlevel(&u.uz))
		    what = "you lose control of",  where = "yourself";
		else
		    what = "you slap against the",  where = surface(u.ux,u.uy);
		pline("The world spins and %s %s.", what, where);
		flags.soundok = 0;
		nomul(-rnd(10));
		nomovemsg = "You are conscious again.";
		afternmv = Hear_again;
		return(1);
	}
	return(0);
}

static int
eatcorpse(otmp)		/* called when a corpse is selected as food */
	register struct obj *otmp;
{
	register const char *cname = mons[otmp->corpsenm].mname;
	register int tp, rotted = 0;

	tp = 0;

	if(otmp->corpsenm != PM_LIZARD) {
#ifndef LINT	/* problem if more than 320K moves before try to eat */
		rotted = (monstermoves - otmp->age)/((long)(10 + rn2(20)));
#endif

		if(otmp->cursed) rotted += 2;
		else if (otmp->blessed) rotted -= 2;
	}

	if(otmp->corpsenm != PM_ACID_BLOB && (rotted > 5)) {
		pline("Ulch - that %s was tainted!",
		      mons[otmp->corpsenm].mlet == S_FUNGUS ? "fungoid vegetation" :
		      is_meaty(&mons[otmp->corpsenm]) ? "meat" : "protoplasm");
#ifdef POLYSELF
		if (u.usym == S_FUNGUS)
			pline("It doesn't seem at all sickening, though...");
		else {
#endif
			make_sick((long) rn1(10, 10),FALSE);
			Sprintf(corpsename, "rotted %s corpse", cname);
			u.usick_cause = (const char *)corpsename;
			flags.botl = 1;
#ifdef POLYSELF
		}
#endif
		if (carried(otmp)) useup(otmp);
		else useupf(otmp);
		return(1);
	} else if(acidic(&mons[otmp->corpsenm])
#ifdef POLYSELF
		  && !resists_acid(uasmon)
#endif
		 ) {
		tp++;
		You("have a very bad case of stomach acid.");
		losehp(rnd(15), "acidic corpse", KILLED_BY_AN);
	} else if(poisonous(&mons[otmp->corpsenm]) && rn2(5)) {
		tp++;
		pline("Ecch - that must have been poisonous!");
		if(!Poison_resistance) {
			losestr(rnd(4));
			losehp(rnd(15), "poisonous corpse", KILLED_BY_AN);
		} else	You("seem unaffected by the poison.");
	/* now any corpse left too long will make you mildly ill */
	} else if(((rotted > 5) || ((rotted > 3) && rn2(5)))
#ifdef POLYSELF
		&& u.usym != S_FUNGUS
#endif
							){
		tp++;
		You("feel%s sick.", (Sick) ? " very" : "");
		losehp(rnd(8), "cadaver", KILLED_BY_AN);
	}
	if(!tp && otmp->corpsenm != PM_LIZARD && (otmp->orotten || !rn2(7))) {
#ifdef POLYSELF
	    if(rottenfood(otmp))
#else
	    if(rottenfood())
#endif
	    {
		otmp->orotten = TRUE;
		(void)touchfood(otmp);
		return(1);
	    }
	    otmp->oeaten >>= 2;
	} else {
#ifdef POLYSELF
	    pline("This %s corpse %s!", cname,
		(carnivorous(uasmon) && !herbivorous(uasmon))
		? "is delicious" : "tastes terrible");
#else
	    pline("This %s corpse tastes terrible!", cname);
#endif
	}

	/* delay is weight dependent */
	victual.reqtime = 3 + (mons[otmp->corpsenm].cwt >> 6);
	return(0);
}

static void
start_eating(otmp)		/* called as you start to eat */
	register struct obj *otmp;
{
#ifdef DEBUG
	debugpline("start_eating: %lx (victual = %lx)", otmp, victual.piece);
	debugpline("reqtime = %d", victual.reqtime);
	debugpline("(original reqtime = %d)", objects[otmp->otyp].oc_delay);
	debugpline("nmod = %d", victual.nmod);
	debugpline("oeaten = %d", otmp->oeaten);
#endif
	victual.fullwarn = victual.doreset = FALSE;
	victual.eating = TRUE;

	if (otmp->otyp == CORPSE) {
	    cprefx(victual.piece->corpsenm);
	    if (!victual.piece && victual.eating) do_reset_eat();
	    if (victual.eating == FALSE) return; /* died and lifesaved */
	}

	if (bite()) return;

	if(++victual.usedtime >= victual.reqtime) {
	    /* print "finish eating" message if they just resumed -dlc */
	    done_eating(victual.reqtime > 1 ? TRUE : FALSE);
	    return;
	}

	Sprintf(msgbuf, "eating %s", the(singular(otmp, xname)));
	set_occupation(eatfood, msgbuf, 0);
}


static void
fprefx(otmp)		/* called on "first bite" of (non-corpse) food */

	register struct obj *otmp;
{
	switch(otmp->otyp) {

	    case FOOD_RATION:
		if(u.uhunger <= 200)
		    if (Hallucination) pline("Oh wow, like, superior, man!");
		    else	       pline("That food really hit the spot!");
		else if(u.uhunger <= 700) pline("That satiated your stomach!");
		break;
	    case TRIPE_RATION:
#ifdef POLYSELF
		if (carnivorous(uasmon) && !humanoid(uasmon))
		    pline("That tripe ration was surprisingly good!");
		else {
#endif
		    pline("Yak - dog food!");
		    more_experienced(1,0);
		    flags.botl = 1;
#ifdef POLYSELF
		}
#endif
		if(rn2(2)
#ifdef POLYSELF
			&& (!carnivorous(uasmon) || humanoid(uasmon))
#endif
						) {
			make_vomiting((long)rn1(victual.reqtime, 14), FALSE);
		}
		break;
#ifdef POLYSELF
	    case CLOVE_OF_GARLIC:
		if (is_undead(uasmon)) {
			make_vomiting((long)rn1(victual.reqtime, 5), FALSE);
			break;
		}
		/* Fall through otherwise */
#endif
	    default:
#ifdef TUTTI_FRUTTI
		if (otmp->otyp==SLIME_MOLD && !otmp->cursed
			&& otmp->spe == current_fruit)
		    pline("My, that was a %s %s!",
			  Hallucination ? "primo" : "yummy",
			  singular(otmp, xname));
		else
#endif
#ifdef UNIX
		if (otmp->otyp == APPLE || otmp->otyp == PEAR) {
		    if (!Hallucination) pline("Core dumped.");
		    else {
/* This is based on an old Usenet joke, a fake a.out manual page */
			int x = rnd(100);
			if (x <= 75)
			    pline("Segmentation fault -- core dumped.");
			else if (x <= 99)
			    pline("Bus error -- core dumped.");
			else pline("Yo' mama -- core dumped.");
		    }
		} else
#endif
		    pline("This %s is %s", singular(otmp, xname),
		      otmp->cursed ? (Hallucination ? "grody!" : "terrible!") :
		      otmp->otyp == CRAM_RATION ? "bland." :
		      Hallucination ? "gnarly!" : "delicious!");
		break;
	}
}

#ifdef POLYSELF
static void
eatring(otmp)
struct obj *otmp;
{
	int typ = otmp->otyp;
	int oldprop;

	/* Note: rings are not so common that this is unbalancing.  (How */
	/* often do you even _find_ 3 rings of polymorph in a game? */
	oldprop = !!(u.uprops[objects[typ].oc_oprop].p_flgs);
	otmp->known = otmp->dknown = 1; /* by taste */
	if (!rn2(3)) switch (otmp->otyp) {
	    case RIN_AGGRAVATE_MONSTER:
	    case RIN_WARNING:
	    case RIN_POISON_RESISTANCE:
	    case RIN_FIRE_RESISTANCE:
	    case RIN_COLD_RESISTANCE:
	    case RIN_SHOCK_RESISTANCE:
	    case RIN_TELEPORTATION:
	    case RIN_TELEPORT_CONTROL:
	    case RIN_SEE_INVISIBLE:
	    case RIN_PROTECTION_FROM_SHAPE_CHAN:
	    case RIN_SEARCHING:
	    case RIN_STEALTH:
	    case RIN_INVISIBILITY:
	    case RIN_CONFLICT:
	    case RIN_POLYMORPH:
	    case RIN_POLYMORPH_CONTROL:
	    case RIN_REGENERATION: /* Probably stupid. */
	    case RIN_HUNGER: /* Stupid. */
	    case RIN_LEVITATION: /* Stupid. */
		if (!(u.uprops[objects[typ].oc_oprop].p_flgs & FROMOUTSIDE))
		    pline("Magic spreads through your body as you digest the ring.");
		u.uprops[objects[typ].oc_oprop].p_flgs |= FROMOUTSIDE;
		if (typ == RIN_SEE_INVISIBLE) {
		    set_mimic_blocking();
		    see_monsters();
		    if (Invis && !oldprop
#ifdef POLYSELF
				&& !perceives(uasmon)
#endif
							&& !Blind) {
			newsym(u.ux,u.uy);
			pline("Suddenly you can see yourself.");
			makeknown(typ);
		    }
		} else if (typ == RIN_INVISIBILITY) {
		    if (!oldprop && !See_invisible && !Blind) {
			newsym(u.ux,u.uy);
			Your("body takes on a %s transparency...",
				Hallucination ? "normal" : "strange");
			makeknown(typ);
		    }
		} else if (typ == RIN_PROTECTION_FROM_SHAPE_CHAN)
		    rescham();
		else if (typ == RIN_LEVITATION) {
		    if (!Levitation) {
			float_up();
			makeknown(typ);
		    }
		}
		break;
	    case RIN_ADORNMENT:
		if (adjattrib(A_CHA, otmp->spe, -1))
		    makeknown(typ);
		break;
	    case RIN_GAIN_STRENGTH:
	    case RIN_INCREASE_DAMAGE: /* Any better ideas? */
		if (adjattrib(A_STR, otmp->spe, -1))
		    makeknown(typ);
		break;
	    case RIN_PROTECTION:
		Protection |= FROMOUTSIDE;
		u.ublessed += otmp->spe;
		flags.botl = 1;
		break;
	}
}

static void
eatspecial() /* called after eating non-food */
{
	register struct obj *otmp = victual.piece;

	lesshungry(victual.nmod);
	victual.piece = (struct obj *)0;
	victual.eating = 0;
	if (otmp->oclass == GOLD_CLASS) {
		dealloc_obj(otmp);
		return;
	}
	if (otmp->oclass == POTION_CLASS) {
		otmp->quan++; /* dopotion() does a useup() */
		(void)dopotion(otmp);
	}
	if (otmp->oclass == RING_CLASS)
		eatring(otmp);
	if (otmp == uball) unpunish();
	if (otmp == uchain) unpunish(); /* but no useup() */
	else if (carried(otmp)) useup(otmp);
	else useupf(otmp);
}

/* NOTE: the order of these words exactly corresponds to the
   order of oc_material values #define'd in objclass.h. */
static const char *foodwords[] = {
	"meal", "liquid", "wax", "food", "meat",
	"paper", "cloth", "leather", "wood", "bone", "scale",
	"metal", "metal", "metal", "silver", "gold", "platinum", "mithril",
	"plastic", "glass", "rich food", "stone"
};

static const char *
foodword(otmp)
register struct obj *otmp;
{
	if (otmp->oclass == FOOD_CLASS) return "food";
	if (otmp->oclass == GEM_CLASS &&
	    objects[otmp->otyp].oc_material == GLASS &&
	    otmp->dknown)
		makeknown(otmp->otyp);
	return foodwords[objects[otmp->otyp].oc_material];
}
#endif

static void
fpostfx(otmp)		/* called after consuming (non-corpse) food */

	register struct obj *otmp;
{
	switch(otmp->otyp) {
#ifdef POLYSELF
	    case SPRIG_OF_WOLFSBANE:
		if (u.ulycn != -1) {
		    u.ulycn = -1;
		    You("feel purified.");
		    if(uasmon == &mons[u.ulycn] && !Polymorph_control)
			rehumanize();
		}
		break;
#endif
	    case CARROT:
		make_blinded(0L,TRUE);
		break;
	    case FORTUNE_COOKIE:
		outrumor(bcsign(otmp), TRUE);
		break;
	    case LUMP_OF_ROYAL_JELLY:
		/* This stuff seems to be VERY healthy! */
		gainstr(otmp, 1);
		u.uhp += (otmp->cursed) ? -rnd(20) : rnd(20);
		if(u.uhp > u.uhpmax) {
			if(!rn2(17)) u.uhpmax++;
			u.uhp = u.uhpmax;
		} else if(u.uhp <= 0) {
			killer_format = KILLED_BY_AN;
			killer = "rotten lump of royal jelly";
			done(POISONING);
		}
		if(!otmp->cursed) heal_legs();
		break;
	    case EGG:
		if(otmp->corpsenm == PM_COCKATRICE) {
#ifdef POLYSELF
		    if(!resists_ston(uasmon))
			if(!poly_when_stoned(uasmon) ||
			   !polymon(PM_STONE_GOLEM))
		    {
#endif
			if (!Stoned) Stoned = 5;
			killer_format = KILLED_BY_AN;
			killer = "cockatrice egg";
#ifdef POLYSELF
		    }
#endif
		}
		break;
	}
	return;
}

int
doeat()		/* generic "eat" command funtion (see cmd.c) */
{
	register struct obj *otmp;
	int basenutrit;			/* nutrition of full item */

	if (Strangled) {
		pline("If you can't breathe air, how can you consume solids?");
		return 0;
	}
	if (!(otmp = floorfood("eat", 0))) return 0;
	if (check_capacity(NULL)) return 0;
#ifdef POLYSELF
	/* We have to make non-foods take 1 move to eat, unless we want to
	 * do ridiculous amounts of coding to deal with partly eaten plate
	 * mails, players who polymorph back to human in the middle of their
	 * metallic meal, etc....
	 */
	if (!is_edible(otmp)) {
	    You("cannot eat that!");
	    return 0;
	}
	if (otmp->oclass != FOOD_CLASS) {
	    victual.reqtime = 1;
	    victual.piece = otmp;
		/* Don't split it, we don't need to if it's 1 move */
	    victual.usedtime = 0;
	    victual.canchoke = (u.uhs == SATIATED);
		/* Note: gold weighs 1 pt. for each 1000 pieces (see */
		/* pickup.c) so gold and non-gold is consistent. */
	    if (otmp->oclass == GOLD_CLASS)
		basenutrit = ((otmp->quan > 200000L) ? 2000
			: (int)(otmp->quan/100L));
	    else if(otmp->oclass == BALL_CLASS || otmp->oclass == CHAIN_CLASS)
		basenutrit = weight(otmp);
	    /* oc_nutrition is usually weight anyway */
	    else basenutrit = objects[otmp->otyp].oc_nutrition;
	    victual.nmod = basenutrit;
	    victual.eating = TRUE; /* needed for lesshungry() */

	    if (otmp->cursed)
		(void) rottenfood(otmp);

	    if (otmp->oclass == WEAPON_CLASS && otmp->opoisoned) {
		pline("Ecch - that must have been poisonous!");
		if(!Poison_resistance) {
		    losestr(rnd(4));
		    losehp(rnd(15), xname(otmp), KILLED_BY_AN);
		} else
		    You("seem unaffected by the poison.");
	    } else if (!otmp->cursed)
		pline("This %s is delicious!",
		      otmp->oclass == GOLD_CLASS ? foodword(otmp) :
		      singular(otmp, xname));
	    eatspecial();
	    return 1;
	}
#endif

	if(otmp == victual.piece) {
	/* If they weren't able to choke, they don't suddenly become able to
	 * choke just because they were interrupted.  On the other hand, if
	 * they were able to choke before, if they lost food it's possible
	 * they shouldn't be able to choke now.
	 */
	    if (u.uhs != SATIATED) victual.canchoke = FALSE;
	    if(!carried(victual.piece)) {
		if(victual.piece->quan > 1L)
			(void) splitobj(victual.piece, 1L);
	    }
	    You("resume your meal.");
	    start_eating(victual.piece);
	    return(1);
	}

	/* nothing in progress - so try to find something. */
	/* tins are a special case */
	if(otmp->otyp == TIN) {
	    start_tin(otmp);
	    return(1);
	}

	victual.piece = otmp = touchfood(otmp);
	victual.usedtime = 0;

	/* Now we need to calculate delay and nutritional info.
	 * The base nutrition calculated here and in eatcorpse() accounts
	 * for normal vs. rotten food.  The reqtime and nutrit values are
	 * then adjusted in accordance with the amount of food left.
	 */
	if(otmp->otyp == CORPSE) {
	    if(eatcorpse(otmp)) return(1);
	    /* else eatcorpse sets up reqtime and oeaten */
	} else {
	    victual.reqtime = objects[otmp->otyp].oc_delay;
	    if (otmp->otyp != FORTUNE_COOKIE &&
		(otmp->cursed ||
		 (((monstermoves - otmp->age) > (int) otmp->blessed ? 50:30) &&
		(otmp->orotten || !rn2(7))))) {

#ifdef POLYSELF
		if(rottenfood(otmp))
#else
		if(rottenfood())
#endif
		{
		    otmp->orotten = TRUE;
		    return(1);
		}
		otmp->oeaten >>= 1;
	    } else fprefx(otmp);
	}

	/* re-calc the nutrition */
	if (otmp->otyp == CORPSE) basenutrit = mons[otmp->corpsenm].cnutrit;
	else basenutrit = objects[otmp->otyp].oc_nutrition;

#ifdef DEBUG
	debugpline("before rounddiv: victual.reqtime == %d", victual.reqtime);
	debugpline("oeaten == %d, basenutrit == %d", otmp->oeaten, basenutrit);
#endif
	victual.reqtime = (basenutrit == 0 ? 0 :
		rounddiv(victual.reqtime * (long)otmp->oeaten, basenutrit));
#ifdef DEBUG
	debugpline("after rounddiv: victual.reqtime == %d", victual.reqtime);
#endif
	/* calculate the modulo value (nutrit. units per round eating)
	 * note: this isn't exact - you actually lose a little nutrition
	 *	 due to this method.
	 * TODO: add in a "remainder" value to be given at the end of the
	 *	 meal.
	 */
	if(victual.reqtime == 0)
	    /* possible if most has been eaten before */
	    victual.nmod = 0;
	else if (otmp->oeaten > victual.reqtime)
	    victual.nmod = -(otmp->oeaten / victual.reqtime);
	else
	    victual.nmod = victual.reqtime % otmp->oeaten;
	victual.canchoke = (u.uhs == SATIATED);

	start_eating(otmp);
	return(1);
}

/* Take a single bite from a piece of food, checking for choking and
 * modifying usedtime.  Returns 1 if they choked and survived, 0 otherwise.
 */
static int
bite()
{
	if(victual.canchoke && u.uhunger >= 2000) {
		choke(victual.piece);
		return 1;
	}
	if (victual.doreset) {
		do_reset_eat();
		return 0;
	}
	if(victual.nmod < 0) {
		lesshungry(-victual.nmod);
		victual.piece->oeaten -= -victual.nmod;
	} else if(victual.nmod > 0 && (victual.usedtime % victual.nmod)) {
		lesshungry(1);
		victual.piece->oeaten--;
	}
	recalc_wt();
	return 0;
}

#endif /* OVLB */
#ifdef OVL0

void
gethungry()	/* as time goes by - called by moveloop() and domove() */
{
	if (u.uinvulnerable) return;	/* you don't feel hungrier */

	if ((!u.usleep || !rn2(10))	/* slow metabolic rate while asleep */
		&& (carnivorous(uasmon) || herbivorous(uasmon)))
	    u.uhunger--;		/* ordinary food consumption */

	if (moves % 2) {	/* odd turns */
	    /* Regeneration uses up food, unless due to an artifact */
	    if ((HRegeneration & (~W_ART)) &&
		(HRegeneration != W_WEP || !uwep->oartifact)) u.uhunger--;
	    if (near_capacity() > SLT_ENCUMBER) u.uhunger--;
	} else {		/* even turns */
	    if (Hunger) u.uhunger--;
	    /* Conflict uses up food too */
	    if ((Conflict & (~W_ARTI))) u.uhunger--;
	    /* +0 charged rings don't do anything, so don't affect hunger */
	    switch ((int)(moves % 20)) {	/* note: use even cases only */
	     case  4: if (uleft &&
			  (uleft->spe || !objects[uleft->otyp].oc_charged))
			    u.uhunger--;
		    break;
	     case  8: if (uamul) u.uhunger--;
		    break;
	     case 12: if (uright &&
			  (uright->spe || !objects[uright->otyp].oc_charged))
			    u.uhunger--;
		    break;
	     case 16: if (u.uhave.amulet) u.uhunger--;
		    break;
	     default: break;
	    }
	}
	newuhs(TRUE);
}

#endif /* OVL0 */
#ifdef OVLB

void
morehungry(num)	/* called after vomiting and after performing feats of magic */
register int num;
{
	u.uhunger -= num;
	newuhs(TRUE);
}


void
lesshungry(num)	/* called after eating (and after drinking fruit juice) */
register int num;
{
#ifdef DEBUG
	debugpline("lesshungry(%d)", num);
#endif
	u.uhunger += num;
	if(u.uhunger >= 2000) {
	    if (!victual.eating || victual.canchoke){
		if (victual.eating) {
			choke(victual.piece);
			reset_eat();
		} else
		if (tin.tin)
			choke(tin.tin);
		else
			choke((struct obj *) 0);
	    }
		/* no reset_eat(); it was a non-food such as juice */
	} else {
	    /* Have lesshungry() report when you're nearly full so all eating
	     * warns when you're about to choke.
	     */
	    if (u.uhunger >= 1500) {
	      if(!victual.eating || (victual.eating && !victual.fullwarn)) {
		pline("You're having a hard time getting all of it down.");
		nomovemsg = "You're finally finished.";
		if(!victual.eating)
		    multi = -2;
		else {
		    victual.fullwarn = TRUE;
		    if (victual.canchoke &&
			/* a one-gulp food will not survive a stop */
				victual.reqtime > 1) {
			if(yn("Stop eating?") == 'y')
			{
				reset_eat();
				nomovemsg = NULL;
			}
		    }
		}
	      }
	    }
	}
	newuhs(FALSE);
}

STATIC_PTR
int
unfaint()
{
	(void) Hear_again();
	if(u.uhs > FAINTING)
		u.uhs = FAINTING;
	stop_occupation();
	flags.botl = 1;
	return 0;
}

#endif /* OVLB */
#ifdef OVL0

boolean
is_fainted()
{
	return((boolean)(u.uhs == FAINTED));
}

void
reset_faint()	/* call when a faint must be prematurely terminated */
{
	if(is_fainted()) nomul(0);
}

#if 0
void
sync_hunger()
{

	if(is_fainted()) {

		flags.soundok = 0;
		nomul(-10+(u.uhunger/10));
		nomovemsg = "You regain consciousness.";
		afternmv = unfaint;
	}
}
#endif

void
newuhs(incr)		/* compute and comment on your (new?) hunger status */
	boolean incr;
{
	register int newhs, h = u.uhunger;

	newhs = (h > 1000) ? SATIATED :
		(h > 150) ? NOT_HUNGRY :
		(h > 50) ? HUNGRY :
		(h > 0) ? WEAK : FAINTING;

	if(newhs == FAINTING) {
		if(is_fainted()) newhs = FAINTED;
		if(u.uhs <= WEAK || rn2(20-u.uhunger/10) >= 19) {
			if(!is_fainted() && multi >= 0 /* %% */) {
				/* stop what you're doing, then faint */
				stop_occupation();
				You("faint from lack of food.");
				flags.soundok = 0;
				nomul(-10+(u.uhunger/10));
				nomovemsg = "You regain consciousness.";
				afternmv = unfaint;
				newhs = FAINTED;
			}
		} else
		if(u.uhunger < -(int)(200 + 20*ACURR(A_CON))) {
			u.uhs = STARVED;
			flags.botl = 1;
			bot();
			You("die from starvation.");
			killer_format = KILLED_BY;
			killer = "starvation";
			done(STARVING);
			/* if we return, we lifesaved, and that calls newuhs */
			return;
		}
	}

	if(newhs != u.uhs) {
		if(newhs >= WEAK && u.uhs < WEAK)
			losestr(1);	/* this may kill you -- see below */
		else if(newhs < WEAK && u.uhs >= WEAK)
			losestr(-1);
		switch(newhs){
		case HUNGRY:
			if (Hallucination) {
			    pline((!incr) ?
				"You now have a lesser case of the munchies." :
				"You are getting the munchies.");
			} else
			    You((!incr) ? "only feel hungry now." :
				  (u.uhunger < 145) ? "feel hungry." :
				   "are beginning to feel hungry.");
			if (incr && occupation &&
			    (occupation != eatfood && occupation != opentin))
			    stop_occupation();
			break;
		case WEAK:
			if (Hallucination)
			    pline((!incr) ?
				  "You still have the munchies." :
      "The munchies are interfering with your motor capabilities.");
			else
			    You((!incr) ? "feel weak now." :
				  (u.uhunger < 45) ? "feel weak." :
				   "are beginning to feel weak.");
			if (incr && occupation &&
			    (occupation != eatfood && occupation != opentin))
			    stop_occupation();
			break;
		}
		u.uhs = newhs;
		flags.botl = 1;
		if(u.uhp < 1) {
			You("die from hunger and exhaustion.");
			killer_format = KILLED_BY;
			killer = "exhaustion";
			done(STARVING);
			return;
		}
	}
}

#endif /* OVL0 */
#ifdef OVLB

/* Returns an object representing food.  Object may be either on floor or
 * in inventory.
 */
struct obj *
floorfood(verb,corpsecheck)	/* get food from floor or pack */
	const char *verb;
	int corpsecheck; /* 0, no check, 1, corpses, 2, tinnable corpses */
{
	register struct obj *otmp;
	char qbuf[QBUFSZ];
	char c;
#ifdef POLYSELF
	struct obj *gold = g_at(u.ux, u.uy);
	boolean feeding = (!strcmp(verb, "eat"));

	if (feeding && gold && metallivorous(uasmon)) {
	    if (gold->quan == 1L)
		Sprintf(qbuf, "There is 1 gold piece here; eat it?");
	    else Sprintf(qbuf, "There are %ld gold pieces here; eat them?",
								gold->quan);
	    if (yn(qbuf) == 'y') {
		/* tricky, because gold isn't a real object -dlc */
		freeobj(gold);
		return gold;
	    }
	}
#endif
	/* Is there some food (probably a heavy corpse) here on the ground? */
	if (!(Levitation && !Is_airlevel(&u.uz)  && !Is_waterlevel(&u.uz))
	    && !u.uswallow) {
	    for(otmp = level.objects[u.ux][u.uy]; otmp; otmp = otmp->nexthere) {
		if(corpsecheck ?
		(otmp->otyp==CORPSE && (corpsecheck == 1 || tinnable(otmp))) :
#ifdef POLYSELF
		    feeding ? (otmp->oclass != GOLD_CLASS && is_edible(otmp)) :
#endif
						otmp->oclass==FOOD_CLASS) {
			Sprintf(qbuf, "There %s %s here; %s %s?",
				(otmp->quan == 1L) ? "is" : "are",
				doname(otmp), verb,
				(otmp->quan == 1L) ? "it" : "one");
			if((c = yn_function(qbuf,ynqchars,'n')) == 'y')
				return(otmp);
			else if(c == 'q')
				return((struct obj *) 0);
		}
	    }
	}
#ifdef POLYSELF
	/* We cannot use ALL_CLASSES since that causes getobj() to skip its
	 * "ugly checks" and we need to check for inedible items.
	 */
	return getobj(feeding ? (const char *)allobj :
				(const char *)comestibles, verb);
#else
	return getobj(comestibles, verb);
#endif
}

/* Side effects of vomiting */
/* added nomul (MRS) - it makes sense, you're too busy being sick! */
/* TO DO: regurgitate swallowed monsters when poly'd */
void
vomit()		/* A good idea from David Neves */
{
	make_sick(0L,TRUE);
	nomul(-2);
}

int
eaten_stat(base, obj)
register int base;
register struct obj *obj;
{
	long uneaten_amt, full_amount;

	uneaten_amt = (long)obj->oeaten;
	full_amount = (obj->otyp == CORPSE) ? (long)mons[obj->corpsenm].cnutrit
					: (long)objects[obj->otyp].oc_nutrition;

	base = (int)(full_amount ? (long)base * uneaten_amt / full_amount : 0L);
	return (base < 1) ? 1 : base;
}

#endif /* OVLB */

/*eat.c*/
