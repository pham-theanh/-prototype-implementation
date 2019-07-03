#include "UnfoldingChecker.hpp"

Transition::Transition(int read_write, int access_var)
{
  this->read_write = read_write;
  this->access_var = access_var;
}

Transition::Transition(int mbId, int commId, string type)
{
  this->mailbox_id = mbId;
  this->commId     = commId;
  this->type       = type;
}

bool Transition::operator<(const Transition& other) const
{

  return ((this->id < other.id) || (this->actor_id < other.actor_id));
}
bool Transition::operator==(const Transition& other) const
{

  return ((this->id == other.id) && (this->actor_id == other.actor_id));
}

bool Transition::isDependent(Transition other)
{
  // two transitions are in the same actor => dependent (causality dependence)
  if (this->actor_id == other.actor_id)
    return true;

  if (this->type == "Isend") {
    if (other.type == "Isend" and (this->mailbox_id == other.mailbox_id))
      return true;
  }

  else if (this->type == "Ireceive") {
    if (other.type == "Ireceive" and (this->mailbox_id == other.mailbox_id))
      return true;
  }

  else if (this->type == "Lock" and other.type == "Lock" and this->mutexID == other.mutexID)
    return true;
  else if (this->type == "Unlock") {
    if (other.type == "Mwait" or other.type == "Mtest")
      return true;
  }

  // for read - write model
  else {

    // if at least one write transition  => dependent
    if (this->access_var == other.access_var)
      if ((this->read_write == 1) or (other.read_write == 1))
        return true;
  }

  return false;
}

Actor::Actor(int id, std::vector<Transition> trans)
{
  this->id       = id;
  this->nb_trans = trans.size();

  for (unsigned int i = 0; i < this->nb_trans; i++) {
    this->trans.push_back(trans[i]);
    this->trans[i].id       = i;
    this->trans[i].actor_id = id;
  }
}

Actor::Actor(int id, unsigned int nb_trans, std::array<Transition, 30>& trans)
{
  this->id       = id;
  this->nb_trans = nb_trans;

  for (unsigned int i = 0; i < nb_trans; i++) {
    this->trans.push_back(trans[i]);
    this->trans[i].id       = i;
    this->trans[i].actor_id = id;
  }
}

bool Actor::operator<(const Actor& other) const
{
  return (this->id < other.id);
}

bool Mailbox::operator<(const Mailbox& other) const
{
  return (this->id < other.id);
}

void Mailbox::update(Transition t)
{
  Communication comm;

  comm.actorId = t.actor_id;
  comm.commId  = t.commId;
  // comm.status="pending";

  if (t.type == "Isend") {
    for (unsigned long i = 0; i < this->nbReceive; i++)
      if (receiveList[i].status == "pending") {
        receiveList[i].status = "ready";
        comm.status           = "ready";

        break;
      }
    sendList[nbSend] = comm;
    this->nbSend++;
  }

  if (t.type == "Ireceive") {
    for (unsigned long i = 0; i < this->nbSend; i++)
      if (sendList[i].status == "pending") {

        sendList[i].status = "ready";
        comm.status        = "ready";

        break;
      }
    receiveList[nbReceive] = comm;
    nbReceive++;
  }
};

bool Mailbox::checkComm(Transition t)
{

  for (unsigned long i = 0; i < nbSend; i++)
    if (t.actor_id == sendList[i].actorId and t.commId == sendList[i].commId and sendList[i].status == "ready")
      return true;

  for (unsigned long i = 0; i < nbReceive; i++)
    if (t.actor_id == receiveList[i].actorId and t.commId == receiveList[i].commId and receiveList[i].status == "ready")
      return true;

  return false;
}

State::State(unsigned long nb_actor, std::set<Actor> actors, std::set<Mailbox> mailboxes)
{
  this->nb_actors_ = nb_actor;
  this->actors_    = actors;
  this->mailboxes_ = mailboxes;
}

/* this function execute a transition from a given state, returning a next state*/
State State::execute(Transition t)
{
  std::set<Actor> actors, actors1;
  std::set<Mailbox> mail_box;

  mail_box = this->mailboxes_;
  actors   = this->actors_;
  actors1  = this->actors_;

  // update the status of the actors of the State, set "executed" = true for the executing transition (t)
  for (auto p : actors)
    if (p.id == t.actor_id) {
      actors1.erase(p);
      p.trans[t.id].executed = true;
      actors1.insert(p);
    }
  /* if t is send or receive transition, then update the mailbox */

  if (t.type == "Isend" or t.type == "Ireceive")
    for (auto mb : mailboxes_)
      if (mb.id == t.mailbox_id) {
        mail_box.erase(mb);
        mb.update(t);
        mail_box.insert(mb);
        break;
      }

  return State(this->nb_actors_, actors1, mail_box);
}

std::set<Transition> State::getEnabledTransition()
{

  std::set<Transition> trans_set;

  for (auto p : this->actors_)
    for (unsigned long j = 0; j < p.nb_trans; j++)
      if (not p.trans[j].executed) {
        // std::cout<< "hien thi not executed : \n";
        // std::cout<< "actorid =" << p.id <<" type ="<< p.trans[j].type <<"   :\n";

        // if transition is Wait, check it's communication is ready ?
        bool chk = true;
        if (p.trans[j].type == "Wait") {

          for (auto mb : this->mailboxes_)
            if (p.trans[j].mailbox_id == mb.id and (not mb.checkComm(p.trans[j])))
              chk = false;
          // else std::cout<<"\n" <<  p.trans[j].type <<" " << p.trans[j].id <<" of P" << p.id <<" not ready :\n" ;
          //	if(chk and p.trans[j].actor_id == 1) std::cout<<" \n thang wait  " << p.trans[j].id <<" cua "
          //<<p.trans[j].actor_id <<" co enabled \n";
        }

        if (chk)
          trans_set.insert(p.trans[j]);

        break;
      }

/*
          std::cout<<"\n enabled transitions : \n";
           for(auto tr : trans_set)
           std::cout<< "actorid =" << tr.actor_id<<" \n id="<< tr.id <<" mailbox="<<tr.mailbox_id<<" comm= " <<tr.commId
           << " type ="<<tr.type <<"\n";
           std::cout<<" \n het ---\n";*/

  return trans_set;
}

void State::print()
{
  std::cout << "s = (";
  for (auto p : this->actors_)
    for (unsigned long j = 0; j < p.nb_trans; j++)
      if (p.trans[j].executed)
        std::cout << "t" << j << "-p" << p.id << " is executed";
  std::cout << ")";
}
