from pathlib import Path
p=Path('outputs/redalert2-recomp/src/linux/singularity.h');s=p.read_text();needle='static void sg_tick(void) {\n#ifdef RA2_TARGET_GAMEMD';s=s.replace(needle,'''static void sg_tick(void) {
#ifdef SG_LAB
    static int reported=0;uint32_t ff=MEM32(RA2_GAME_FRAME_VA);
    if(ff>30&&!reported){reported=1;fprintf(stderr,"[lab] mode=%u on=%d houses=%d buildings=%d\\n",MEM32(0xA8B238),sg_on(),sg_vec_count(0xA80228),sg_vec_count(0xA8EB40));for(int i=0;i<sg_vec_count(0xA8EB40);i++){uint32_t b=sg_vec_at(0xA8EB40,i);fprintf(stderr,"[lab] building=%s alive=%d owner=%08X\\n",sg_id(b,1),sg_alive(b),MEM32(b+0x21C));}}
#endif
#ifdef RA2_TARGET_GAMEMD''');p.write_text(s)
p=Path('work/singularity/build_lab.py');s=p.read_text().replace("'-DSG_RESEARCH_TICKS=90'","'-DSG_LAB=1','-DSG_RESEARCH_TICKS=90'").replace("lab/build/ra2'", "lab/build/ra2-check'");p.write_text(s)
