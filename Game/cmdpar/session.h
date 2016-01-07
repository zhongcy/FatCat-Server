#ifndef SESSION_H
#define SESSION_H

#include <boost/unordered_map.hpp>
#include <boost/noncopyable.hpp>
#include <boost/array.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>

#include <cstdio>

#include "Game/postgresqlstruct.h"
#include "NetWork/tcpconnection.h"
#include "Game/log.hpp"
#include "Game/cmdtypes.h"

#include "Game/rolestruct.h"

using boost::asio::ip::tcp;

class Interchange
{
public:
    Interchange():isInchange(false),isLocked(false),isChangeChecked(false){}
    bool isInchange;                       //是否在交易
    bool isLocked;                           //锁定状态  如果锁定则锁定方不能再选择物品
    bool isChangeChecked;             //是否已经确认交易
    TCPConnection::Pointer partnerConn;       //交易的对方连接
    int roleId;

    std::vector<STR_Goods> changes;    //缓存交易过程中要交易物品
    vector<STR_EquipmentAttr> vecEqui; //缓存交易过程中交易的装备的信息
    interchangeMoney money;   //缓存交易过程中要交易的金钱

    bool inChange()
    {
        return isInchange;
    }

    void goInChange()
    {
        isInchange = true;
    }

    void goOutChange()
    {
        isInchange = false;
    }

    void lock()                                             //锁定 不再改变交易物品
    {
        isLocked = true;
    }

    void unlock()                                       //解除锁定，可以改变交易物品（一方解除锁定，则另一方也变为解除锁定状态）
    {
        isLocked = false;
        isChangeChecked = false;
    }

    void checkChange()                             //确认交易  如果双方都确认交易 则交易成功
    {
        isChangeChecked = true;
    }

    void clear()                                           //恢复到原来未交易状态
    {
        isInchange = false;
        isLocked =false;
        isChangeChecked = false;
        partnerConn.reset();
        changes.clear();
        money.MoneyCount = 0;

    }
};

class Session
{
public:
    Session();
    ~Session()
    {

    }

    typedef boost::array<char,40>    Buff;
    Buff                    m_usrid;             //用户名
    hf_uint32               m_roleid;            //角色ID
    hf_double               m_publicCoolTime;    //公共冷却时间
    hf_double               m_skillUseTime;      //再次使用技能的时间
    STR_RoleInfo            m_roleInfo;          //角色攻击属性
    STR_PackRoleExperience  m_roleExp;           //角色经验
    STR_RoleBasicInfo       m_RoleBaseInfo;      //角色基本信息
    STR_BodyEquipment       m_BodyEqu;           //角色所穿戴装备
    umap_taskProcess        m_playerAcceptTask;  //玩家已接取的任务
    umap_friendList         m_friendList;        //好友列表
    umap_roleSock           m_viewRole;          //可视范围内的玩家
    umap_playerViewMonster  m_viewMonster;       //可视范围内的怪物
    umap_skillTime          m_skillTime;         //保存玩家所有技能的再次使用时间
    umap_roleGoods          m_playerGoods;       //玩家背包其他物品
    umap_roleEqu            m_playerEqu;         //玩家背包装备
//    umap_roleEquAttr        m_playerEquAttr;     //玩家背包装备属性
    umap_roleMoney          m_playerMoney;       //玩家金币
    umap_completeTask       m_completeTask;      //玩家已经完成的任务
    STR_PackPlayerPosition  m_position;          //位置
    umap_lootGoods          m_lootGoods;         //掉落物品
    umap_lootPosition       m_lootPosition;      //掉落物品位置，时间
    boost::shared_ptr<Interchange> m_interchage;
    hf_char                 m_goodsPosition[BAGCAPACITY];   //保存玩家物品位置
    STR_PlayerStartPos      m_StartPos;          //保存玩家刷新数据起始点
    umap_taskGoods          m_taskGoods;         //保存玩家任务物品
    hf_double               m_commonAttackTime;  //下一次普通攻击的时间

};



class SessionMgr:private boost::noncopyable,public boost::enable_shared_from_this<SessionMgr>
{
public:

    typedef boost::unordered_map<TCPConnection::Pointer,Session>    SessionMap;
    typedef boost::shared_ptr<SessionMap>           SessionPointer;
    typedef boost::shared_ptr<SessionMgr>            Pointer;

    typedef boost::unordered_map<tcp::socket*, TCPConnection::Pointer > sockMap;

    typedef boost::unordered_map<string,TCPConnection::Pointer> _umap_nickSock;
    typedef boost::shared_ptr<_umap_nickSock> umap_nickSock;



    sockMap     m_sockMap;

    void    SaveSession(TCPConnection::Pointer sock,Session& ss)
    {
           SessionMgr::SessionMap *ssmap = m_sessions.get();
            (*ssmap)[sock] = ss;
    }
     void    SaveSession(TCPConnection::Pointer sock,char *name)
     {
         Session s;
         sprintf(&(s.m_usrid[0]),name);
         s.m_roleid = 0;
//        SessionMgr::SessionMap *ssmap = m_sessions.get();

        SessionsAdd(sock, s);
//         (*ssmap)[sock] = s;
     }

     void SaveSession(TCPConnection::Pointer sock, STR_PackPlayerPosition* playerPosition)
     {
          SessionMgr::SessionMap* ssmap = m_sessions.get();
          memcpy(&(*ssmap)[sock].m_position, playerPosition, sizeof(STR_PackPlayerPosition));
     }

     void SaveSession(TCPConnection::Pointer conn, hf_int32 roleid)
     {
        SessionMgr::SessionMap *ssmap = m_sessions.get();
         (*ssmap)[conn].m_roleid = roleid;

        (*m_roleSock)[roleid] = conn;
     }

     void SaveSession(TCPConnection::Pointer sock, STR_RoleInfo* roleInfo)
     {
         SessionMgr::SessionMap *ssmap = m_sessions.get();
         memcpy(&(*ssmap)[sock].m_roleInfo, roleInfo, sizeof(STR_RoleInfo));
     }


     void   RemoveSession( TCPConnection::Pointer sk)
     {
       Logger::GetLogger()->Info("Remove Session");

       SessionMgr::SessionMap::iterator it = m_sessions->find(sk);
       if(it != m_sessions->end())
       {
           m_sessions->erase(it);
       }
     }

    char *GetUserBySock(TCPConnection::Pointer sk )
    {
        return    &   (*(m_sessions.get()))[sk].m_usrid[0];
    }

    ~SessionMgr()               {}

    static    Pointer             Instance()
    {
         static Pointer           m_inst = Pointer(new SessionMgr());
//        if ( m_inst.get() == NULL )
//            m_inst = Pointer(new SessionMgr());
        return m_inst;
    }


    SessionPointer             GetSession()
    {
        return m_sessions;
    }

    umap_roleSock             GetRoleSock()
    {
        return m_roleSock;
    }

    umap_nickSock GetNickSock()
    {
        return m_nickSock;
    }

    umap_nickSock GetNameSock()
    {
        return m_nameSock;
    }

    void SessionsAdd(TCPConnection::Pointer conn, Session s)
    {
        m_sessionsMtx.lock();
        cout << conn->socket().native_handle() << endl;
        cout << "m_sessions add start:" << m_sessions->size() << endl;
        SessionMap::iterator it = m_sessions->find(conn);
        if(it == m_sessions->end())
            (*m_sessions)[conn] = s;
        else
            Logger::GetLogger()->Error("sessionsAdd error");
        cout << "m_sessions add end:" << m_sessions->size() << endl << endl << endl;
        m_sessionsMtx.unlock();
    }

    void SessionsErase(TCPConnection::Pointer conn)
    {
        m_sessionsMtx.lock();
        cout << "m_sessions delete start:" << m_sessions->size() << endl;
        SessionMap::iterator it = m_sessions->find(conn);
        if(it != m_sessions->end())
            m_sessions->erase(it);
        else
            Logger::GetLogger()->Error("sessionsErase error");
        cout << "m_sessions delete end:" << m_sessions->size() << endl;
        m_sessionsMtx.unlock();
    }

    void RoleSockAdd(hf_uint32 roleid, TCPConnection::Pointer conn)
    {
        m_roleMtx.lock();
        cout << "addstart" << m_roleSock->size() << endl;
        _umap_roleSock::iterator it = m_roleSock->find(roleid);
        if(it == m_roleSock->end())
            (*m_roleSock)[roleid] = conn;
        else
            Logger::GetLogger()->Error("roleSockAdd error");
        cout << "add end" << m_roleSock->size() << endl;
        m_roleMtx.unlock();
    }

    void RoleSockErase(hf_uint32 roleid)
    {
        m_roleMtx.lock();
        _umap_roleSock::iterator it = m_roleSock->find(roleid);
        if(it != m_roleSock->end())
            m_roleSock->erase(roleid);
        else
            Logger::GetLogger()->Error("roleSockErase error");
        m_roleMtx.unlock();
    }

    void NickSockAdd(hf_char* nick, TCPConnection::Pointer conn)
    {
        m_nickMtx.lock();
        _umap_nickSock::iterator it = m_nickSock->find(nick);
        if(it == m_nickSock->end())
            (*m_nickSock)[nick] = conn;
        else
            Logger::GetLogger()->Error("nickSockAdd error");
        m_nickMtx.unlock();
    }

    void NickSockErase(hf_char* nick)
    {
        m_nickMtx.lock();
        _umap_nickSock::iterator it = m_nickSock->find(nick);
        if(it != m_nickSock->end())
            m_nickSock->erase(nick);
        else
            Logger::GetLogger()->Error("nickSockErase error");
        m_nickMtx.unlock();
    }

    void NameSockAdd(hf_char* name, TCPConnection::Pointer conn)
    {
        m_nameMtx.lock();
        _umap_nickSock::iterator it = m_nameSock->find(name);
        if(it == m_nameSock->end())
            (*m_nameSock)[name] = conn;
        else
            Logger::GetLogger()->Error("nameSockAdd error");        
        m_nameMtx.unlock();
    }

    void NameSockErase(hf_char* name)
    {
        m_nameMtx.lock();
        _umap_nickSock::iterator it = m_nameSock->find(name);
        if(it != m_nameSock->end())
            m_nameSock->erase(name);
        else
        {
//            printf("要清除的用户名为：%s\n", name);
            Logger::GetLogger()->Error("name not find,erase error");
        }
        m_nameMtx.unlock();
    }

    int addRef(int tp  = 0){
        static int tms = 0;
        if( 0 == tp){
        tms++;
        }
        return tms;
    }

private:

    SessionMgr():
      m_sessions(new SessionMap())
      ,m_roleSock(new _umap_roleSock)
      ,m_nickSock(new _umap_nickSock)
      ,m_nameSock(new _umap_nickSock)
    {
        cout<<"\n===================Create SessionMgr================="<<endl;
    }

    SessionPointer              m_sessions;
    umap_roleSock               m_roleSock;
    umap_nickSock               m_nickSock;
    umap_nickSock               m_nameSock;

    boost::mutex                m_sessionsMtx;
    boost::mutex                m_roleMtx;
    boost::mutex                m_nickMtx;
    boost::mutex                m_nameMtx;



};


#endif // SESSION_H
