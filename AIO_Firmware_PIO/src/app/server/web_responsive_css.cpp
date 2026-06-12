#include "web_responsive_css.h"

#include <Arduino.h>

static const char WEB_RESPONSIVE_CSS[] PROGMEM = R"CSS(
@media (max-width: 860px){
html,body{height:auto;min-height:100%;}
body{overflow:auto;}
.app{display:flex;flex-direction:column;width:100%;min-height:100vh;height:auto;}
.sidebar{position:sticky;top:0;z-index:20;min-height:0;border-right:0;border-bottom:1px solid var(--border);}
.brand{padding:12px 14px;}
.brand-mark{width:28px;height:28px;border-radius:7px;}
.brand-name{font-size:13px;}
.brand-version{font-size:10px;}
.nav-group{flex:0 0 auto;display:flex;overflow-x:auto;overflow-y:hidden;gap:6px;padding:8px;scrollbar-width:none;}
.nav-group::-webkit-scrollbar{display:none;}
.nav-label{display:none;}
.nav-item{flex:0 0 auto;padding:8px 10px;white-space:nowrap;font-size:12px;}
.sidebar-foot{padding:8px 12px;}
.topbar{height:auto;min-height:48px;padding:10px 14px;align-items:center;flex-wrap:wrap;}
.crumbs{font-size:11px;}
.status-cluster{width:100%;margin-left:0;justify-content:flex-start;overflow-x:auto;}
.pill{flex:0 0 auto;max-width:100%;}
.content{padding:18px 14px 28px;overflow:visible;}
.page-head{margin-bottom:14px;align-items:flex-start;}
.page-title{font-size:22px;}
.page-sub{font-size:12px;}
.card,.hero{border-radius:10px;margin-bottom:14px;}
.card-head{padding:14px;align-items:flex-start;flex-wrap:wrap;}
.card-body{padding:0;}
.field{grid-template-columns:1fr;align-items:stretch;gap:8px;padding:12px 14px;}
.field label{font-size:12px;}
.field input[type=text],.field input[type=password],.field input[type=number],.field select,.field input[type=file]{height:40px;font-size:16px;}
.field .tip{justify-self:start;}
.tip:hover::after{right:auto;left:0;top:calc(100% + 8px);transform:none;max-width:min(280px,calc(100vw - 32px));white-space:normal;}
.row-actions{padding:14px;flex-wrap:wrap;}
.row-actions .btn,.row-actions button{flex:1 1 140px;justify-content:center;}
.btn{min-height:40px;}
.hero{padding:18px;}
.hero h2{font-size:16px;}
.hero .kpi-grid{grid-template-columns:repeat(2,minmax(0,1fr));gap:12px;}
.hero .kpi .value{font-size:22px;}
.scan-row{grid-template-columns:minmax(0,1fr) auto;gap:8px;padding:11px 14px;}
.scan-row .meta{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
.toast{left:14px;right:14px;bottom:14px;}
}
@media (max-width: 420px){
.topbar{padding:9px 12px;}
.content{padding:14px 10px 24px;}
.page-title{font-size:20px;}
.card-head{padding:12px;}
.field{padding:11px 12px;}
.row-actions{padding:12px;}
.status-cluster{gap:6px;}
.pill{padding:5px 8px;}
.lang-pills a{padding:4px 8px;}
.hero .kpi-grid{grid-template-columns:1fr;}
}
)CSS";

const char *web_responsive_css(void)
{
    return WEB_RESPONSIVE_CSS;
}
